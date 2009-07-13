/***********************************************************************
 Moses - factored phrase-based language decoder
 Copyright (C) 2009 University of Edinburgh
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ***********************************************************************/

#include <algorithm>
#include <functional>
#include <iostream>
#include <iomanip>
#include <fstream>

#ifdef MPI_ENABLED
#include <mpi.h>
#endif

#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "AnnealingSchedule.h"
#include "QuenchingSchedule.h"
#include "Decoder.h"
#include "Dependency.h"
#include "Derivation.h"
#include "Gibbler.h"
#include "InputSource.h"
#include "OnlineLearnerTrainer.h"
#include "GibbsOperator.h"
#include "SentenceBleu.h"
#include "GainFunction.h"
#include "GibblerMaxTransDecoder.h"
#include "MpiDebug.h"
#include "Model1.h"
#include "Pos.h"
#include "SourceToTargetRatio.h"
#include "Timer.h"
#include "StaticData.h"
#include "OnlineLearner.h"
#include "SampleAcceptor.h"

#if 0
vector<string> refs;
refs.push_back("export of high-tech products in guangdong in first two months this year reached 3.76 billion us dollars");
refs.push_back("guangdong's export of new high technology products amounts to us $ 3.76 billion in first two months of this year");
refs.push_back("guangdong exports us $ 3.76 billion worth of high technology products in the first two months of this year");
refs.push_back("in the first 2 months this year , the export volume of new hi-tech products in guangdong province reached 3.76 billion us dollars .");
SentenceBLEU sb(4, refs);
vector<const Factor*> fv;
GainFunction::ConvertStringToFactorArray("guangdong's new high export technology comes near on $ 3.76 million in two months of this year first", &fv);
cerr << sb.ComputeGain(fv) << endl;
#endif
using namespace std;
using namespace Josiah;
using namespace Moses;
using boost::lexical_cast;
using boost::bad_lexical_cast;
using boost::split;
using boost::is_any_of;
namespace po = boost::program_options;


void LoadReferences(const vector<string>& ref_files, GainFunctionVector* g, float bp_scale = 1.0, bool use_bp_denum_hack = false) {
  assert(ref_files.size() > 0);
  vector<ifstream*> ifs(ref_files.size(), NULL);
  for (unsigned i = 0; i < ref_files.size(); ++i) {
    cerr << "Reference " << (i+1) << ": " << ref_files[i] << endl;
    ifs[i] = new ifstream(ref_files[i].c_str());
    assert(ifs[i]->good());
  }
  while(!ifs[0]->eof()) {
    vector<string> refs(ref_files.size());
    for (unsigned int i = 0; i < refs.size(); ++i) {
      getline(*ifs[i], refs[i]);
    }
    if (refs[0].empty() && ifs[0]->eof()) break;
    g->push_back(new SentenceBLEU(4, refs, bp_scale, use_bp_denum_hack));
  }
  for (unsigned i=0; i < ifs.size(); ++i) delete ifs[i];
  cerr << "Loaded reference translations for " << g->size() << " sentences." << endl;
}

/**
 * Wrap moses timer to give a way to no-op it.
 **/
class GibbsTimer {
public:
  GibbsTimer() : m_doTiming(false) {}
  void on() {m_doTiming = true; m_timer.start("TIME: Starting timer");}
  void check(const string& msg) {if (m_doTiming) m_timer.check(string("TIME:" + msg).c_str());}
private:
  Timer m_timer;
  bool m_doTiming;
} timer;

void configure_features_from_file(const std::string& filename, feature_vector& fv){
  std::cerr << "Reading extra features from " << filename << std::endl;
  std::ifstream in(filename.c_str());
  if (!in) {
    throw std::runtime_error("Unable to open feature configuration file");
  }
  // todo: instead of having this function know about all required options of
  // each feature, have features populate options / read variable maps /
  // populate feature_vector using static functions.
  po::options_description desc;
  bool useApproxPef = false;
  bool useApproxPfe = false;
  bool useVerbDiff = false;
  bool useCherry = false;
  bool useDepDist = false;
  bool useSrcTgtRatio = false;
  size_t dependencyFactor;
  desc.add_options()
  ("model1.table", "Model 1 table")
  ("model1.pef_column", "Column containing p(e|f) score")
  ("model1.pfe_column", "Column containing p(f|e) score")
  ("model1.approx_pef",po::value<bool>(&useApproxPef)->default_value(false), "Approximate the p(e|f), and use importance sampling")
  ("model1.approx_pfe",po::value<bool>(&useApproxPfe)->default_value(false), "Approximate the p(f|e), and use importance sampling")
  ("pos.verbdiff", po::value<bool>(&useVerbDiff)->default_value(false), "Verb difference feature")
  ("dependency.cherry", po::value<bool>(&useCherry)->default_value(false), "Use Colin Cherry's syntactic cohesiveness feature")
  ("dependency.distortion", po::value<bool>(&useDepDist)->default_value(false), "Use the dependency distortion feature")
  ("dependency.factor", po::value<size_t>(&dependencyFactor)->default_value(1), "Factor representing the dependency tree")
  ("srctgtratio.useFeat", po::value<bool>(&useSrcTgtRatio)->default_value(false), "Use source length to target length ratio feature");
  
  po::variables_map vm;
  po::store(po::parse_config_file(in,desc,true), vm);
  notify(vm);
  if (!vm["model1.pef_column"].empty() || !vm["model1.pfe_column"].empty()){
    boost::shared_ptr<external_model1_table> ptable;
    boost::shared_ptr<moses_factor_to_vocab_id> p_evocab_mapper;
    boost::shared_ptr<moses_factor_to_vocab_id> p_fvocab_mapper;
    if (vm["model1.table"].empty())
      throw std::runtime_error("Requesting Model 1 features, but no Model 1 table given");
    else {
      ptable.reset(new external_model1_table(vm["model1.table"].as<std::string>()));
      p_fvocab_mapper.reset(new moses_factor_to_vocab_id(ptable->f_vocab(), Moses::Input, 0, Moses::FactorCollection::Instance())); 
      p_evocab_mapper.reset(new moses_factor_to_vocab_id(ptable->e_vocab(), Moses::Output, 0, Moses::FactorCollection::Instance())); 
    }
    if (!vm["model1.pef_column"].empty()) {
      if (useApproxPef) {
        cerr << "Using approximation for model1" << endl;
        fv.push_back(feature_handle(new ApproximateModel1(ptable, p_fvocab_mapper, p_evocab_mapper)));
      } else {
        fv.push_back(feature_handle(new model1(ptable, p_fvocab_mapper, p_evocab_mapper)));
      }
    }
    if (!vm["model1.pfe_column"].empty()) {
      if (useApproxPfe) {
        cerr << "Using approximation for model1 inverse" << endl;
        fv.push_back(feature_handle(new ApproximateModel1Inverse(ptable, p_fvocab_mapper, p_evocab_mapper)));
      } else {
        fv.push_back(feature_handle(new model1_inverse(ptable, p_fvocab_mapper, p_evocab_mapper)));
      }
    }
    
  }
  if (useVerbDiff) {
    //FIXME: Should be configurable
    fv.push_back(feature_handle(new VerbDifferenceFeature(1,1)));
  }
  if (useCherry) {
    fv.push_back(feature_handle(new CherrySyntacticCohesionFeature(dependencyFactor)));
  }
  if (useSrcTgtRatio) {
    fv.push_back(feature_handle(new SourceToTargetRatio));
  }
  if (useDepDist) {
    fv.push_back(feature_handle(new DependencyDistortionFeature(dependencyFactor)));
  }
  in.close();
}




/**
 * Main for Josiah - the Gibbs sampler for moses.
 **/
int main(int argc, char** argv) {
  int rank = 0, size = 1;
#ifdef MPI_ENABLED
  MPI_Init(&argc,&argv);
  MPI_Comm comm = MPI_COMM_WORLD;
  MPI_Comm_rank(comm,&rank);
  MPI_Comm_size(comm,&size);
  cerr << "MPI rank: " << rank << endl; 
  cerr << "MPI size: " << size << endl;
#endif
  string stopperConfig;
  unsigned int topn;
  int debug;
  int mpidebug;
  string mpidebugfile;
  int burning_its;
  int mbr_size;
  string inputfile;
  string outputfile;
  string mosesini;
  bool decode;
  bool translate;
  bool translation_distro;
  bool derivation_distro;
  bool help;
  
  unsigned training_batch_size;
  
  bool do_timing;
  bool show_features;
  uint32_t seed;
  int lineno;
  bool randomize;
  float scalefactor;
  string weightfile;
  vector<string> ref_files;
  bool decode_monotone;
  bool decode_zero_weights;
  bool decode_nolm;
  bool decode_random;
  int periodic_decode;
  bool collect_dbyt;
  bool output_max_change;
  bool anneal;
  unsigned int reheatings;
  float max_temp;
  vector<float> prev_gradient;
  float brev_penalty_scaling_factor;
  bool hack_bp_denum;
  int weight_dump_freq;
  string weight_dump_stem;
  bool sampleRank;
  bool perceptron, mira;
  float perceptron_lr;
  int epochs;
  bool greedy, fixedTemp;
  float fixed_temperature;
  po::options_description desc("Allowed options");
  desc.add_options()
  ("help",po::value( &help )->zero_tokens()->default_value(false), "Print this help message and exit")
  ("config,f",po::value<string>(&mosesini),"Moses ini file")
  ("verbosity,v", po::value<int>(&debug)->default_value(0), "Verbosity level")
  ("mpi-debug", po::value<int>(&MpiDebug::verbosity)->default_value(0), "Verbosity level for debugging messages used in mpi.")
  ("mpi-debug-file", po::value<string>(&mpidebugfile), "Debug file stem for use by mpi processes")
  ("random-seed,e", po::value<uint32_t>(&seed), "Random seed")
  ("timing,m", po::value(&do_timing)->zero_tokens()->default_value(false), "Display timing information.")
  ("iterations,s", po::value<string>(&stopperConfig)->default_value("5"), 
   "Sampler stopping criterion (eg number of iterations)")
  ("burn-in,b", po::value<int>(&burning_its)->default_value(1), "Duration (in sampling iterations) of burn-in period")
  ("scale-factor,c", po::value<float>(&scalefactor)->default_value(1.0), "Scale factor for model weights.")
  ("decode-monotone", po::value(&decode_monotone)->zero_tokens()->default_value(false), "Run the initial decoding monotone.")
  ("decode-nolm", po::value(&decode_nolm)->zero_tokens()->default_value(false), "Run the initial decoding without an lm.")
  ("decode-zero-weights", po::value(&decode_zero_weights)->zero_tokens()->default_value(false), "Run the initial decoding with weights set to zero.")
  ("decode-random", po::value(&decode_random)->zero_tokens()->default_value(false), "Use the random decoder.")
  ("input-file,i",po::value<string>(&inputfile),"Input file containing tokenised source")
  ("output-file-prefix,o",po::value<string>(&outputfile),"Output file prefix for translations, MBR output, etc")
  ("nbest-drv,n",po::value<unsigned int>(&topn)->default_value(0),"Write the top n derivations to stdout")
	("show-features,F",po::value<bool>(&show_features)->zero_tokens()->default_value(false),"Show features and then exit")
	("weights,w",po::value<string>(&weightfile),"Weight file")
  ("decode-derivation,d",po::value( &decode)->zero_tokens()->default_value(false),"Write the most likely derivation to stdout")
  ("decode-translation,t",po::value(&translate)->zero_tokens()->default_value(false),"Write the most likely translation to stdout")
  ("distro-derivation", po::value(&derivation_distro)->zero_tokens()->default_value(false), "Print derivation probability distribution")
  ("distro-translation", po::value(&translation_distro)->zero_tokens()->default_value(false), "Print translation probability distribution")
  ("periodic-derivation,p",po::value(&periodic_decode)->default_value(0), "Periodically write the max derivation to stderr")
  ("max-change", po::value(&output_max_change)->zero_tokens()->default_value(false), "Whenever the max deriv or max trans changes, write it to stderr")
  ("collect-dbyt",po::value(&collect_dbyt)->zero_tokens()->default_value(false), "Collect derivations per translation")
  ("line-number,L", po::value(&lineno)->default_value(0), "Starting reference/line number")
  ("randomize-batches,R", po::value(&randomize)->zero_tokens()->default_value(false), "Randomize training batches")
  ("training-batch-size,S", po::value(&training_batch_size)->default_value(0), "Batch size to use during online training, 0 = full corpus")
	("reheatings", po::value<unsigned int>(&reheatings)->default_value(1), "Number of times to reheat the sampler")
	("anneal,a", po::value(&anneal)->default_value(false)->zero_tokens(), "Use annealing during the burn in period")
	("max-temp", po::value<float>(&max_temp)->default_value(4.0), "Annealing maximum temperature")
  ("ref,r", po::value<vector<string> >(&ref_files), "Reference translation files for training")
  ("extra-feature-config,X", po::value<string>(), "Configuration file for extra (non-Moses) features")
  ("hack-bp-denum,H", po::value(&hack_bp_denum)->default_value(false), "Use a predefined scalar as denum in BP computation")
  ("bp-scale,B", po::value<float>(&brev_penalty_scaling_factor)->default_value(1.0f), "Scaling factor for sent level brevity penalty for BLEU - default is 1.0")
  ("weight-dump-freq", po::value<int>(&weight_dump_freq)->default_value(0), "Frequency to dump weight files during training")
  ("weight-dump-stem", po::value<string>(&weight_dump_stem)->default_value("weights"), "Stem of filename to use for dumping weights")
  ("sample-rank", po::value(&sampleRank)->zero_tokens()->default_value(false), "Train using sample Rank")
  ("perceptron", po::value(&perceptron)->zero_tokens()->default_value(false), "Use perceptron learner")
  ("mira", po::value(&mira)->zero_tokens()->default_value(false), "Use mira learner")
  ("perc-lr", po::value<float>(&perceptron_lr)->default_value(1.0f), "Perceptron learning rate")
  ("epochs", po::value<int>(&epochs)->default_value(1), "Number of training epochs")
  ("greedy", po::value(&greedy)->default_value(false), "Greedy sample acceptor")
  ("fixed-temp-accept", po::value(&fixedTemp)->default_value(false), "Fixed temperature sample acceptor")
  ("fixed-temperature", po::value<float>(&fixed_temperature)->default_value(1.0f), "Temperature for fixed temp sample acceptor");
  
  
  po::options_description cmdline_options;
  cmdline_options.add(desc);
  po::variables_map vm;
  po::store(po::command_line_parser(argc,argv).
            options(cmdline_options).run(), vm);
  po::notify(vm);
  
  feature_vector extra_features; 
  if (!vm["extra-feature-config"].empty()){
    configure_features_from_file(vm["extra-feature-config"].as<std::string>(), extra_features);
  }
  std::cerr << "Using " << extra_features.size() << " extra features" << std::endl;
  
  if (help) {
    std::cout << "Usage: " + string(argv[0]) +  " -f mosesini-file [options]" << std::endl;
    std::cout << desc << std::endl;
    return 0;
  }
  
  if (translation_distro) translate = true;
  if (derivation_distro) decode = true;
  
  if (mosesini.empty()) {
    cerr << "Error: No moses ini file specified" << endl;
    return 1;
  }
  
  if (mpidebugfile.length()) {
    MpiDebug::init(mpidebugfile,rank);
  }
   
  if (do_timing) {
    timer.on();
  }
  
  if (!mira && !perceptron) {
    cerr << "Error: No learning algorithm chosen" << endl;
    return 1;
  }
  
  if (mira && perceptron) {
    cerr << "Error: Choose just one learning algorithm" << endl;
    return 1;
  }
  
  //set up moses
  initMoses(mosesini,weightfile,debug);
  auto_ptr<Decoder> decoder;
  if (decode_random) {
    if (decode_monotone || decode_zero_weights || decode_nolm) {
      cerr << "Error:: Random decoder cannot be used with any other options." << endl;
#ifdef MPI_ENABLED
      MPI_Finalize();
#endif
      return -1;
    }
    decoder.reset(new RandomDecoder());
  } else {
    decoder.reset(new MosesDecoder());
  }
  
  // may be invoked just to get a features list
  if (show_features) {
    OutputWeights(cout);
#ifdef MPI_ENABLED
    MPI_Finalize();
#endif
    return 0;
  }
  
  if (decode_monotone) {
    decoder->SetMonotone(true);
  }
  
  if (decode_zero_weights) {
    decoder->SetZeroWeights(true);
  }
  
  if (decode_nolm) {
    decoder->SetNoLM(true);
  }
  
  //scale model weights
  vector<float> weights = StaticData::Instance().GetAllWeights();
  transform(weights.begin(),weights.end(),weights.begin(),bind2nd(multiplies<float>(),scalefactor));
  const_cast<StaticData&>(StaticData::Instance()).SetAllWeights(weights);
  VERBOSE(1,"Scaled weights by factor of " << scalefactor << endl);
  
  if (vm.count("random-seed")) {
    RandomNumberGenerator::instance().setSeed(seed + rank);
  }      
  
  GainFunctionVector g;
  if (ref_files.size() > 0) LoadReferences(ref_files, &g, brev_penalty_scaling_factor, hack_bp_denum);
  
  ostream* out = &cout;
  if (!outputfile.empty()) {
    ostringstream os;
    os << setfill('0');

    os << outputfile << '.' << setw(3) << rank << "_of_" << size;
    VERBOSE(1, "Writing output to: " << os.str() << endl);
    out = new ofstream(os.str().c_str());
  }
  
  auto_ptr<AnnealingSchedule> annealingSchedule;
  if (anneal) {
    annealingSchedule.reset(new LinearAnnealingSchedule(burning_its, max_temp));  
  }
  
  Sampler sampler;
  sampler.SetAnnealingSchedule(annealingSchedule.get());
  VERBOSE(2,"Reheatings: " << reheatings << endl);
  sampler.SetReheatings(reheatings);
  
  //configure the sampler
  MergeSplitOperator mso;
  FlipOperator fo;
  TranslationSwapOperator tso;
    
  sampler.AddOperator(&mso);
  sampler.AddOperator(&tso);
  sampler.AddOperator(&fo);
  
  //Acceptor
  auto_ptr<SampleAcceptor> acceptor;
  if (greedy || fixed_temperature == 0) {
    acceptor.reset(new GreedyAcceptor());
  }
  else if (fixedTemp){
    acceptor.reset(new FixedTempAcceptor(fixed_temperature));
  }
  else {
    acceptor.reset(new RegularAcceptor);
  }
  
  
  //Add the learner
  auto_ptr<OnlineLearner> onlineLearner;
  mira = true;
  if (perceptron) {
    onlineLearner.reset(new PerceptronLearner(StaticData::Instance().GetWeights(), "Perceptron", perceptron_lr));
  }
  else if (mira) {
    onlineLearner.reset(new MiraLearner(StaticData::Instance().GetWeights(), "MIRA"));
  }

  sampler.AddOnlineLearner(onlineLearner.get());
  
  //sampler stopping strategy; TODO: push parsing of config into StoppingStrategy ctor ?
  auto_ptr<StopStrategy> stopper;
  try {
    size_t iterations = lexical_cast<size_t>(stopperConfig);
    stopper.reset(new CountStopStrategy(iterations));
  } catch (bad_lexical_cast&) {/* do nothing*/}
  
  
  if (!stopper.get()) {
    cerr << "Error: unable to parse stopper config string '" << stopperConfig << "'" << endl;
    exit(1);
  }
  
  sampler.SetStopper(stopper.get());
  sampler.SetBurnIn(burning_its);
  
  
  auto_ptr<istream> in;
  auto_ptr<InputSource> input;
  
  vector<string> input_lines;
  ifstream infiles(inputfile.c_str());
  assert (infiles);
  while(infiles) {
    string line;
    getline(infiles, line);
    if (line.empty() && infiles.eof()) break;
    assert(!line.empty());
    input_lines.push_back(line);
  }
  VERBOSE(1, "Loaded " << input_lines.size() << " lines in training mode" << endl);
  if (!training_batch_size || training_batch_size > input_lines.size())
    training_batch_size = input_lines.size();
  VERBOSE(1, "Batch size: " << training_batch_size << endl);
  OnlineLearnerTrainer* trainer = new OnlineLearnerTrainer(rank, size, training_batch_size, &input_lines, seed, randomize, epochs * input_lines.size(),     
                                                           weight_dump_freq, weight_dump_stem, onlineLearner.get());
  input.reset(trainer);
  
  timer.check("Processing input file");
  while (input->HasMore()) {
    string line;
    input->GetSentence(&line, &lineno);
    if (line.empty()) {
      if (!input->HasMore()) continue;
      assert(!"I don't like empty lines");
    }
    
    Hypothesis* hypothesis;
    TranslationOptionCollection* toc;
    
    timer.check("Running decoder");
    
    std::vector<Word> source;
    decoder->decode(line,hypothesis,toc,source);
    timer.check("Running sampler");
    
    TranslationDelta::lmcalls = 0;

    if (sampleRank) {
     mso.SetGainFunction(&g[lineno]);
     tso.SetGainFunction(&g[lineno]);
     fo.SetGainFunction(&g[lineno]);
    }

    sampler.Run(hypothesis,toc,source,extra_features,acceptor.get());
    VERBOSE(1, "Language model calls: " << TranslationDelta::lmcalls << endl);
    timer.check("Outputting results");
    cerr << "Curr Weights : " << StaticData::Instance().GetWeights() << endl;
  
    ++lineno;
    
    if (trainer && trainer->GetCurr() == trainer->GetCurrEnd()) {
      trainer->ReserveNextBatch();
    }
  } 
  
  ScoreComponentCollection finalAvgWeights = onlineLearner->GetAveragedWeights();
  //cerr << "Final Weights: " << finalAvgWeights << endl;
  stringstream s;
  s << weight_dump_stem;
  s << "_final";
  string weight_file = s.str();
  cerr << "Dumping final weights to  " << weight_file << endl;
  ofstream f_out(weight_file.c_str());
  
  if (f_out) {
    OutputWeights(finalAvgWeights.data(), f_out);
    f_out.close();
  }  else {
    cerr << "Failed to dump weights" << endl;
  }
  
#ifdef MPI_ENABLED
  MPI_Finalize();
#endif
  (*out) << flush;
  if (!outputfile.empty())
    delete out;
  return 0;
}

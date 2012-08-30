#include <fstream>
#include <sstream>
#include <math.h>
#include <boost/algorithm/string.hpp> 

#include "FFState.h"
#include "LazyMDI.h"
#include "StaticData.h"
#include "LM/Factory.h"

using namespace std;

namespace Moses
{

LazyMDI::LazyMDI(float weight, LMImplementation adaptLMImpl, vector<FactorType> adaptFactorTypes,
    size_t adaptOrder, string adaptFilePath, LanguageModel* baseLM, int contextSize, float adaptMagnitude)
{
  const_cast<ScoreIndexManager&> \
    (StaticData::Instance().GetScoreIndexManager()) \
    .AddScoreProducer(this);
  m_adaptModelType = adaptLMImpl;
  m_adaptFactorType = adaptFactorTypes;
  m_adaptOrder = adaptOrder;
  m_adaptFile = adaptFilePath;
  m_contextSize = contextSize;
  m_backgroundLM = baseLM;
  m_adaptMagnitude = adaptMagnitude;

  LoadAdaptFilePaths();

//  m_mgr = new ScoreIndexManager();
//  m_scoreIndexManager.InitFeatureNames();

  // TODO: (nickruiz) LazyMDI - only one weight is allowed, thus the constructor only allows a scalar float value.
  vector<float> weights;
  weights.push_back(weight);
  const_cast<StaticData&>(StaticData::Instance()).SetWeightsForScoreProducer(this, weights);
}

void LazyMDI::LoadAdaptFilePaths()
{
  ifstream ifs(m_adaptFile.c_str());
  stringstream buffer;
  buffer << ifs.rdbuf();


  string filepath;
  while (getline(buffer, filepath))
  {
    VERBOSE(3, "filepath: " << filepath << endl);
//    m_adaptFilePaths.push_back(const_cast<string*>(&filepath));
    m_adaptFilePaths.push_back(filepath);
  }
}

void LazyMDI::LoadAdaptLM(const string &adaptFile)
{

  if (m_cache.adaptLMPath != adaptFile)
  {
    delete m_cache.adaptLM;
    m_cache.adaptLM = LanguageModelFactory::CreateLanguageModel(
                 m_adaptModelType,
                 m_adaptFactorType,
                 m_adaptOrder,
                 adaptFile,
                 m_cache.scoreIndexManager,
  //               m_scoreIndexManager,
  //               const_cast<ScoreIndexManager&>(StaticData::Instance().GetScoreIndexManager()),
                 0);

    m_cache.adaptLMPath = adaptFile;
  }

}

LazyMDI::~LazyMDI()
{
  // TODO Auto-generated destructor stub
  delete m_backgroundLM;
}

std::string LazyMDI::GetScoreProducerDescription(unsigned) const
{
  return "LazyMDI";
}

std::string LazyMDI::GetScoreProducerWeightShortName(unsigned) const
{
  return "lazymdi";
}

size_t LazyMDI::GetNumScoreComponents() const
{
  return 1;
}

float LazyMDI::SigmoidLog(float x, float magnitude) const
{
  // return 2.0 / (1.0 + exp(-1.0 * x));
//  return magnitude / (1.0 + exp(-1.0 * sharpness * x)) + xShift;
  return magnitude / (1.0 + exp(-1.0 * x));
}

float LazyMDI::FastSigmoid(float x, float magnitude) const
{
  return magnitude * x / (magnitude + abs(x) - 1.0);
}

FFState* LazyMDI::Evaluate( const Hypothesis& cur_hypo,
                         const FFState* prev_state,
                         ScoreComponentCollection* accumulator) const
{
  const Phrase &targetPhrase = cur_hypo.GetCurrTargetPhrase();

  float adaptFullScore, adaptNGramScore;
  size_t adaptOOVCount;

  float bgFullScore, bgNGramScore;
  size_t bgOOVCount;

  float totalScore = 0.0f;
  float score = 0.0f;

  // Extract words from targetPhrase
//  std::vector<Word>  words;
  size_t numWords = targetPhrase.GetSize();
  Phrase tmpPhrase(1);

  string lowercase;
  Word lowercasedWord;
  std::vector<FactorType> wordFactors;

  wordFactors.push_back(0);

  for (int i = 0; i < numWords; i++)
  {
    tmpPhrase.Clear();
    Word word = targetPhrase.GetWord(i);
//    words.push_back(targetPhrase.GetWord(i));
//    tmpPhrase.AddWord(words[i]);
    
    // Only compute lowercased unigram ratios. 
    // We may not have sufficient statistics for cased words in our adaptation text.
    lowercase = word.GetString(wordFactors, false);
    VERBOSE(3, "Old word: " << lowercase);
    boost::algorithm::to_lower(lowercase);
    VERBOSE(3, "\tNew word: " << lowercase << endl);
    lowercasedWord.CreateFromString(Input, StaticData::Instance().GetInputFactorOrder(), lowercase, false);
    tmpPhrase.AddWord(lowercasedWord);

    m_cache.adaptLM->CalcScore(tmpPhrase, adaptFullScore, adaptNGramScore, adaptOOVCount);
    m_backgroundLM->CalcScore(tmpPhrase, bgFullScore, bgNGramScore, bgOOVCount);

    if (adaptOOVCount > 0)
    {
      
      VERBOSE(1, "OOV: `" << lowercase << "` " << adaptOOVCount << endl);
      // OOV words in the adaptation model should not be penalized.
      // Instead, fix the unigram ratio to 1.
      // Since f(1) = 1, don't bother passing it through the sigmoid function.
      score = 1.0;
    }
    else
    {
      score = adaptFullScore - bgFullScore;

      // TODO: (nickruiz) Pick one!
      if (m_adaptMagnitude == 2.0)
      {
        score = SigmoidLog(score, m_adaptMagnitude);
      }
      else
      {
        score = FastSigmoid(exp(score), m_adaptMagnitude);
      }
      // score = SigmoidLog(score, 2.0);
      // score = FastSigmoid(exp(score), 2.0);

    }

    totalScore += log(score);
  }

  accumulator->PlusEquals( this, totalScore );
  return NULL;
}

FFState* LazyMDI::EvaluateChart( const ChartHypothesis& cur_hypo,
                         int featureID,
                         ScoreComponentCollection* accumulator) const
{
  return NULL;
}

const FFState* LazyMDI::EmptyHypothesisState(const InputType& input) const
{
  return NULL;
}

void LazyMDI::InitializeForInput(const InputType& input)
{
  VERBOSE(3, "LazyMDI::InitializeForInput - begin\n");
  m_local.reset(new ThreadLocalStorage);

  size_t transId = input.GetTranslationId();
  VERBOSE(1, "input.GetTranslationId(): " << transId << std::endl);

  // TODO: (nickruiz) Warning: It's possible to go out of bounds here.
  const string& adaptLMPath = (m_contextSize == 0)
      ? m_adaptFilePaths[0]
      : m_adaptFilePaths[(int) min(floor(transId / m_contextSize), m_adaptFilePaths.size() - 1.0)];

//  // TODO: Don't use a hardcoded value
//  const string& adaptLMPath = m_adaptFilePaths[0];
  VERBOSE(1, " adaptLMPath = " << adaptLMPath << endl);

  // TODO: (nickruiz) For now, try loading a single LM automatically.
  LoadAdaptLM(adaptLMPath);

  VERBOSE(3, "LazyMDI::InitializeForInput - end\n");
}

}

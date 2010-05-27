#include "CorpusSampler.h"


#include "GainFunction.h"
#include "Hypothesis.h"
#include "Decoder.h"
#include "SentenceBleu.h"
#include "GibbsOperator.h"
#ifdef MPI_ENABLED
#include <mpi.h>
#endif

using namespace std;

namespace Josiah {
  
void CorpusSamplerCollector::collect(Sample& s) {
   //nothing to do  
}

//Resample based on derivation distribution
void CorpusSamplerCollector::resample(int sent) {
  std::map<const Derivation*,double> m_p, m_resampled_p;
  m_derivationCollector.getDistribution(m_p); //fetch the distribution
    
  //copy it to a vector, will be easier for further processing
  vector<const Derivation*> derivations;
  vector<double> scores;
    
  for (map<const Derivation*,double>::iterator it = m_p.begin(); it != m_p.end(); ++it) {
    derivations.push_back(it->first);
    scores.push_back(it->second);
  }
  
  //Printing out distribution

  IFVERBOSE(2) {
    for (size_t i = 0; i < derivations.size();++i) {
      cerr << *derivations[i] << " has score " << scores[i] <<endl;
    }  
  }
  
  
  double sum = scores[0];
  for (size_t i = 1; i < scores.size(); ++i) {
    sum = log_sum(sum,scores[i]);
  }
  
  transform(scores.begin(),scores.end(),scores.begin(),bind2nd(minus<double>(),sum));
  
  
  //now sample from this
  for (int j = 0; j < m_samples; ++j) {
    
    //random number between 0 and 1
    double random =  RandomNumberGenerator::instance().next();//(double)rand() / RAND_MAX;
      
    random = log(random);
      
    //now figure out which sample
    size_t position = 1;
    sum = scores[0];
    for (; position < scores.size() && sum < random; ++position) {
      sum = log_sum(sum,scores[position]);
    }
      
    size_t chosen =  position-1;  
    MPI_VERBOSE(2, "Chosen derivation " << chosen << endl)
      
    //Store chosen derivation's feature values and length
    const Derivation* chosenDeriv = derivations[chosen]; 
    m_resampled_p[chosenDeriv] += 1.0/m_samples;
    MPI_VERBOSE(2, "Chosen deriv " << *chosenDeriv << endl)
    MPI_VERBOSE(2, "Chosen deriv size" << chosenDeriv->getTargetSentenceSize() << endl)
    
    m_featureVectors.at(j).PlusEquals(chosenDeriv->getFeatureValues());
    MPI_VERBOSE(2, "Feature vector : " << m_featureVectors.at(j) << endl)
    m_lengths[j] += chosenDeriv->getTargetSentenceSize();
    MPI_VERBOSE(2, "Lengths : " << m_lengths.at(j) << endl)
      
    //Store chosen derivation's gain sufficient stats
    SufficientStats *stats = new BleuSufficientStats(4);
    
    std::vector<const Factor*> yield;
    chosenDeriv->getTargetFactors(yield);
    g[sent]->GetSufficientStats(yield, stats);
    
    m_sufficientStats[j] += *(static_cast<BleuSufficientStats*>(stats));
    MPI_VERBOSE(2, "Stats : " << m_sufficientStats.at(j) << endl)
    delete stats;
  }    
  
  IFVERBOSE(2) {
  cerr << "After resampling, distribution is : " << endl;
  for (map<const Derivation*,double>::iterator it = m_resampled_p.begin(); it != m_resampled_p.end(); ++it) {
    cerr << *(it->first) << "has score " << it->second << endl;
  }
  }
  
  setRegularisation(m_p);
  setRegularisationGradientFactor(m_p);
  
  //Now reset the derivation collector
  m_derivationCollector.reset();
  m_numSents++;
  
}

  
  
#ifdef MPI_ENABLED    
  
void CorpusSamplerCollector::AggregateSamples(int rank) {
  AggregateSuffStats(rank);
}
   
void CorpusSamplerCollector::AggregateSuffStats(int rank) {
  /*what do we need to store?
  1. Feature Vectors
  2. Lengths
  3. Bleu Stats 
  */
  vector  <int>  lengths (m_lengths.size());
  vector< float  > featsVecs, recFeatsVecs;
  vector< float  > suffStats, recSuffStats;
  int numSents;
  
  //Reduce length
  if (MPI_SUCCESS != MPI_Reduce(const_cast<int*>(&m_lengths[0]), &lengths[0], m_lengths.size(), MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD)) MPI_Abort(MPI_COMM_WORLD,1);

  //Reduce numSents
  if (MPI_SUCCESS != MPI_Reduce(&m_numSents, &numSents, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD)) MPI_Abort(MPI_COMM_WORLD,1);
  
  //Reduce feature vectors and sufficient stats
  //MPI can't handle vector of vectors, so first concatenate elements together
  
  //Concatenate feature vectors
  for (size_t i = 0; i < m_featureVectors.size(); ++i) {
    for (size_t j = 0; j < m_featureVectors[i].size(); ++j) {
      featsVecs.push_back(m_featureVectors[i][j]);
    }
  }
  
  //Concatenate sufficient stats 
  for (size_t i = 0; i < m_sufficientStats.size(); ++i) {
    vector < float > bleuStats = m_sufficientStats[i].data();
    for (size_t j = 0; j < bleuStats.size(); ++j) {
      suffStats.push_back(bleuStats[j]);
    }
  }
 
  if (rank == 0) {
    recFeatsVecs.resize(featsVecs.size());
    recSuffStats.resize(suffStats.size());
  }

  //Reduce FVs and SStats
  if (MPI_SUCCESS != MPI_Reduce(const_cast<float*>(&featsVecs[0]), &recFeatsVecs[0], featsVecs.size(), MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD)) MPI_Abort(MPI_COMM_WORLD,1);
  if (MPI_SUCCESS != MPI_Reduce(const_cast<float*>(&suffStats[0]), &recSuffStats[0], suffStats.size(), MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD)) MPI_Abort(MPI_COMM_WORLD,1);

  
  //Unpack FVs and SStats, 
  if (rank == 0 ) {  
    //FVs
    size_t numFeats = recFeatsVecs.size() /  m_featureVectors.size();
    m_featureVectors.clear();

    for (size_t i = 0; i  < recFeatsVecs.size(); i += numFeats) {
      vector<float> features(recFeatsVecs.begin() + i, recFeatsVecs.begin() + i + numFeats);
      ScoreComponentCollection feats(features); 
      m_featureVectors.push_back(feats);   
    }

    //Suff Stats
    size_t sizeStats = recSuffStats.size() /  m_sufficientStats.size();
    m_sufficientStats.clear();
    
    for (size_t i = 0; i  < recSuffStats.size(); i += sizeStats) {
      vector<float> _stats(recSuffStats.begin() + i, recSuffStats.begin() + i + sizeStats);
      BleuSufficientStats stats(_stats);
      m_sufficientStats.push_back(stats);
    }
    
    //Transfer lengths back
    m_lengths = lengths;
    m_numSents = numSents;
  }
}
#endif
  
float CorpusSamplerCollector::UpdateGradient(ScoreComponentCollection* gradient,float *exp_len, float *unreg_exp_gain) {
  ScoreComponentCollection feature_expectations = getFeatureExpectations();
    
  MPI_VERBOSE(1,"FEXP: " << feature_expectations << endl)
    
  //gradient computation
  ScoreComponentCollection grad;
  double exp_gain = 0;
  float gain = 0.0;
  double iweight = 1.0 / m_featureVectors.size();
  for (size_t i = 0; i < m_featureVectors.size() ; ++i) {
    ScoreComponentCollection fv = m_featureVectors[i];
    MPI_VERBOSE(2,"FV: " << fv)
    gain = SentenceBLEU::CalcBleu(m_sufficientStats[i], false);
    fv.MinusEquals(feature_expectations);
    MPI_VERBOSE(2,"DIFF: " << fv)
    fv.MultiplyEquals(gain);
    MPI_VERBOSE(2,"GAIN: " << gain  << endl)
    exp_gain += gain*iweight;
    fv.MultiplyEquals(iweight);
    grad.PlusEquals(fv);
    MPI_VERBOSE(2,"grad: " << grad << endl)
  }
  
  cerr << "Gradient without reg " << grad << endl;
  ScoreComponentCollection regularizationGrad = getRegularisationGradientFactor();
  regularizationGrad.DivideEquals(GetNumSents());
  grad.PlusEquals(regularizationGrad);
  
  
  cerr << "Exp gain without reg term :  " << exp_gain << endl;
  *unreg_exp_gain = exp_gain;
  exp_gain += getRegularisation()/GetNumSents();
  cerr << "Exp gain with reg term:  " << exp_gain << endl;
  
  gradient->PlusEquals(grad);
  MPI_VERBOSE(1,"Gradient: " << grad << endl)
  
  cerr << "Gradient: " << grad << endl;
  
  //expected length
 if (exp_len) {
    *exp_len = 0;
    for (size_t j = 0; j < m_sufficientStats.size(); ++j) {
      *exp_len += m_sufficientStats[j].hyp_len;
    }
  } 
  return exp_gain;
}
  
ScoreComponentCollection CorpusSamplerCollector::getFeatureExpectations() const {
  //do calculation at double precision to try to maintain accuracy
  vector<double> sum(StaticData::Instance().GetTotalScoreComponents());
  double iweight = 1.0/m_featureVectors.size();
  for (size_t i = 0; i < m_featureVectors.size(); ++i) {
    ScoreComponentCollection fv = m_featureVectors[i];
    for (size_t j = 0; j < fv.size(); ++j) {
      sum[j] += fv[j]*iweight;
    }
  }
  vector<float> truncatedSum(sum.size());
  for (size_t i = 0; i < sum.size(); ++i) {
    truncatedSum[i] = static_cast<float>(sum[i]);
  }
  return ScoreComponentCollection(truncatedSum);
}
  
void CorpusSamplerCollector::reset() {
  m_featureVectors.clear(); m_featureVectors.resize(m_samples);
  m_lengths.clear(); m_lengths.resize(m_samples);
  m_sufficientStats.clear(); m_sufficientStats.resize(m_samples);
  m_numSents = 0;
}
  
float CorpusSamplerCollector::getReferenceLength() {
  float refLen(0.0); 
  for (size_t j = 0; j < m_sufficientStats.size(); ++j) {
    refLen += m_sufficientStats[j].ref_len;
  }
  return refLen;
}
  
}
#ifndef moses_LazyMDI_h
#define moses_LazyMDI_h

#include "ScoreProducer.h"
#include "FeatureFunction.h"
#include "TypeDef.h"
#include "LM/Base.h"
#include "ScoreIndexManager.h"

#ifdef WITH_THREADS
#include <boost/thread/tss.hpp>
#endif

using namespace std;

namespace Moses
{

class LazyMDI : public StatefulFeatureFunction
{

  struct ThreadLocalStorage
  {
    const InputType* input;
    LanguageModel* adaptLM;
    string adaptLMPath;
    ScoreIndexManager scoreIndexManager;
//
//    ThreadLocalStorage()
//    {
////      scoreIndexManager.InitFeatureNames();
//    }
//    ~ThreadLocalStorage()
//    {
//      delete adaptLM;
//    }
  };

protected:
  int m_contextSize;
  LMImplementation m_adaptModelType;
  vector<FactorType> m_adaptFactorType;
  size_t m_adaptOrder;
  std::string m_adaptFile;
//  vector<const string*> m_adaptFilePaths;
  vector<string> m_adaptFilePaths;
  LanguageModel* m_backgroundLM;


//  ScoreIndexManager       m_scoreIndexManager;
  ThreadLocalStorage m_cache;

  void LoadAdaptFilePaths();
  // float SigmoidLog(float x, float magnitude, float sharpness, float xShift) const;
  float SigmoidLog(float x, float magnitude) const;
  float FastSigmoid(float x, float magnitude) const;

#ifdef WITH_THREADS
  boost::thread_specific_ptr<ThreadLocalStorage> m_local;
#else
  std::auto_ptr<ThreadLocalStorage> m_local;
#endif
	
public:
  // enum SigmoidType {SigmoidLog, FastSigmoid};
//  LazyMDI(const std::vector<float> &weights);
  LazyMDI(float weight, LMImplementation adaptLMImpl, vector<FactorType> adaptFactorTypes,
      size_t adaptOrder, string adaptFilePath, LanguageModel* baseLM, int contextSize);
  virtual ~LazyMDI();

  size_t GetNumScoreComponents() const;
  std::string GetScoreProducerDescription(unsigned) const;
  std::string GetScoreProducerWeightShortName(unsigned) const;

  virtual FFState* Evaluate( const Hypothesis&, const FFState*, ScoreComponentCollection* ) const;
  virtual FFState* EvaluateChart( const ChartHypothesis&, int, ScoreComponentCollection* ) const;
  const FFState* EmptyHypothesisState(const InputType& input) const;
  void InitializeForInput(const InputType& input);
  void LoadAdaptLM(const string &adaptFile);
};

}

#endif /* moses_LazyMDI_h */

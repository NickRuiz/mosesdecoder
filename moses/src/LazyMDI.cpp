#include <fstream>
#include <sstream>
#include <math.h>

#include "FFState.h"
#include "LazyMDI.h"
#include "StaticData.h"
#include "LM/Factory.h"

using namespace std;

namespace Moses
{

LazyMDI::LazyMDI(float weight, LMImplementation adaptLMImpl, vector<FactorType> adaptFactorTypes,
    size_t adaptOrder, string adaptFilePath, LanguageModel* baseLM, int contextSize)
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

FFState* LazyMDI::Evaluate( const Hypothesis& cur_hypo,
                         const FFState* prev_state,
                         ScoreComponentCollection* accumulator) const
{
  const Phrase &targetPhrase = cur_hypo.GetCurrTargetPhrase();

  float adaptFullScore, adaptNGramScore;
  size_t adaptOOVCount;

  float bgFullScore, bgNGramScore;
  size_t bgOOVCount;

  float score = -1.0f;

  // Compute the unigram LM scores on the target phrase
  m_cache.adaptLM->CalcScore(targetPhrase, adaptFullScore, adaptNGramScore, adaptOOVCount);
  m_backgroundLM->CalcScore(targetPhrase, bgFullScore, bgNGramScore, bgOOVCount);
   score = adaptFullScore - bgFullScore;

  // VERBOSE(1, "LazyMDI adapt - bg: " << adaptFullScore << " - " << bgFullScore << " = " << score << std::endl);

  accumulator->PlusEquals( this, score );
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
  VERBOSE(3, "input.GetTranslationId(): " << transId << std::endl);

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

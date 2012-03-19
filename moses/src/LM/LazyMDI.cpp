#include "FFState.h"
#include "LM/LazyMDI.h"
#include "StaticData.h"

namespace Moses
{

LazyMDI::LazyMDI(const std::vector<float> &weights)
{
  // TODO Auto-generated constructor stub
  const_cast<ScoreIndexManager&> \
    (StaticData::Instance().GetScoreIndexManager()) \
    .AddScoreProducer(this);
  const_cast<StaticData&>(StaticData::Instance()) \
    .SetWeightsForScoreProducer(this, weights);
}

LazyMDI::~LazyMDI()
{
  // TODO Auto-generated destructor stub
}

std::string LazyMDI::GetScoreProducerDescription() const
{
  return "LazyMDIFeature";
}

std::string LazyMDI::GetScoreProducerWeightShortName() const
{
  return "mdi";
}

size_t LazyMDI::GetNumScoreComponents() const
{
  return 1;
}

FFState* LazyMDI::Evaluate( const Hypothesis& cur_hypo,
                         const FFState* prev_state,
                         ScoreComponentCollection* accumulator) const
{
  accumulator->PlusEquals( this, -1.0 );
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

}

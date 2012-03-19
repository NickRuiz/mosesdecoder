#ifndef moses_LazyMDI_h
#define moses_LazyMDI_h

#include "ScoreProducer.h"
#include "FeatureFunction.h"

namespace Moses
{

class LazyMDI : public StatefulFeatureFunction
{
public:
  LazyMDI(const std::vector<float> &weights);
  virtual ~LazyMDI();

  size_t GetNumScoreComponents() const;
  std::string GetScoreProducerDescription(unsigned) const;
  std::string GetScoreProducerWeightShortName(unsigned) const;

  FFState* Evaluate( const Hypothesis&, const FFState*, ScoreComponentCollection* ) const;
  FFState* EvaluateChart( const ChartHypothesis&, int, ScoreComponentCollection* ) const;
  const FFState* EmptyHypothesisState(const InputType& input) const;
};

}

#endif /* moses_LazyMDI_h */

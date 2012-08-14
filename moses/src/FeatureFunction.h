#ifndef moses_FeatureFunction_h
#define moses_FeatureFunction_h

#include <vector>

#include "ScoreProducer.h"

namespace Moses
{

class TargetPhrase;
class Hypothesis;
class ChartHypothesis;
class FFState;
class InputType;
class ScoreComponentCollection;

/** base class for all feature functions.
 * @todo is this for pb & hiero too?
 * @todo what's the diff between FeatureFunction and ScoreProducer?
 */
class FeatureFunction: public ScoreProducer
{

public:
  virtual bool IsStateless() const = 0;
  virtual ~FeatureFunction();

};

/** base class for all stateless feature functions.
 * eg. phrase table, word penalty, phrase penalty
 */
class StatelessFeatureFunction: public FeatureFunction
{

public:
  //! Evaluate for stateless feature functions. Implement this.
  virtual void Evaluate(
    const TargetPhrase& cur_hypo,
    ScoreComponentCollection* accumulator) const;

  // If true, this value is expected to be included in the
  // ScoreBreakdown in the TranslationOption once it has been
  // constructed.
  // Default: true
  virtual bool ComputeValueInTranslationOption() const;

  bool IsStateless() const;
};

/** base class for all stateful feature functions.
 * eg. LM, distortion penalty 
 */
class StatefulFeatureFunction: public FeatureFunction
{

public:

  /**
   * \brief This interface should be implemented.
   * Notes: When evaluating the value of this feature function, you should avoid
   * calling hypo.GetPrevHypo().  If you need something from the "previous"
   * hypothesis, you should store it in an FFState object which will be passed
   * in as prev_state.  If you don't do this, you will get in trouble.
   */
  virtual FFState* Evaluate(
    const Hypothesis& cur_hypo,
    const FFState* prev_state,
    ScoreComponentCollection* accumulator) const = 0;

  virtual FFState* EvaluateChart(
    const ChartHypothesis& /* cur_hypo */,
    int /* featureID */,
    ScoreComponentCollection* accumulator) const = 0;

  //! return the state associated with the empty hypothesis for a given sentence
  virtual const FFState* EmptyHypothesisState(const InputType &input) const = 0;

  virtual void InitializeForInput(const InputType &input) {}

  bool IsStateless() const;
};

}

#endif

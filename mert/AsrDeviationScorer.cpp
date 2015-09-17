#pragma once

#include "AsrDeviationScorer.h"
#include "CderScorer.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

using namespace std;

namespace MosesTuning
{

AsrDeviationScorer::AsrDeviationScorer(const string& config)
	: StatisticsBasedScorer("ASRDevRate", config)
{
	m_wer_hyp_ref = new CderScorer(config, false);
	m_wer_hyp_src = new CderScorer(config, false);
	m_wer_src_ref = new CderScorer(config, false);
}

AsrDeviationScorer::~AsrDeviationScorer() {}

void AsrDeviationScorer::setSourceFile(const string& sourceFile)
{
	m_sourceFile = sourceFile;
}

void AsrDeviationScorer::setReferenceFiles(const vector<string>& referenceFiles)
{
	vector<string> x;
	x.push_back(m_sourceFile);
	const vector<string> sourceFiles = x;
	m_wer_hyp_ref->setReferenceFiles(referenceFiles);
	m_wer_src_ref->setReferenceFiles(referenceFiles);
	m_wer_hyp_src->setReferenceFiles(sourceFiles);

	// make sure source data is clear
	m_src_sentences.clear();

  // load source data
  ifstream srcin(m_sourceFile.c_str());
  if (!srcin) {
  	throw runtime_error("Unable to open: " + m_sourceFile);
  }
  string line;
  while (getline(srcin, line)) {
  	line = this->preprocessSentence(line);
//  	sent_t encoded;
  	//TokenizeAndEncode(line, encoded);
  	m_src_sentences.push_back(line);
  }

  //make sure reference data is clear
  m_ref_sentences.clear();

  //load reference data
  for (size_t rid = 0; rid < referenceFiles.size(); ++rid) {
    ifstream refin(referenceFiles[rid].c_str());
    if (!refin) {
      throw runtime_error("Unable to open: " + referenceFiles[rid]);
    }
    m_ref_sentences.push_back(vector<sent_t>());
    string line;
    while (getline(refin,line)) {
      line = this->preprocessSentence(line);
      sent_t encoded;
      TokenizeAndEncode(line, encoded);
      m_ref_sentences[rid].push_back(encoded);
    }
  }
}

void AsrDeviationScorer::prepareStats(size_t sid, const string& text, ScoreStats& entry)
{
  string sentence = this->preprocessSentence(text);

  vector<ScoreStatsType> stats;
  ScoreStats tmpEntry(1);

  tmpEntry.clear();
  m_wer_hyp_ref->prepareStats(sid, text, tmpEntry);
  stats.push_back(tmpEntry.get(0));

  tmpEntry.clear();
  m_wer_hyp_src->prepareStats(sid, text, tmpEntry);
  stats.push_back(tmpEntry.get(0));

  // TODO: Move to setReferenceFiles. Precompute this.
  tmpEntry.clear();
  m_wer_src_ref->prepareStats(sid, m_src_sentences[sid], tmpEntry);
  stats.push_back(tmpEntry.get(0));

  entry.set(stats);
}

float AsrDeviationScorer::calculateScore(const vector<ScoreStatsType>& comps) const
{
  if (comps.size() != 3) {
    throw runtime_error("Size of stat vector for ASRDevRate is not 3");
  }
  return sqrt(comps[0]*comps[0] + pow(comps[1]- comps[2], 2));
}

}

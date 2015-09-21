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

void AsrDeviationScorer::setSourceFile(string& sourceFile)
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

  ScoreStats tmpEntry(1);
  vector<ScoreStatsType> stats;

  tmpEntry.clear();
  m_wer_hyp_ref->prepareStats(sid, text, tmpEntry);
  stats.push_back(tmpEntry.get(0));
  stats.push_back(tmpEntry.get(1));

  // TODO: Move to setReferenceFiles. Precompute this.
  tmpEntry.clear();
  m_wer_src_ref->prepareStats(sid, m_src_sentences[sid], tmpEntry);
  stats.push_back(tmpEntry.get(0));
	stats.push_back(tmpEntry.get(1));

	tmpEntry.clear();
  m_wer_hyp_src->prepareStats(sid, text, tmpEntry);
  stats.push_back(tmpEntry.get(0));
	stats.push_back(tmpEntry.get(1));

	entry.set(stats);
}

float AsrDeviationScorer::calculateScore(const vector<ScoreStatsType>& comps) const
{
	/*
	cerr << comps[0] << " " << comps[1] << endl
	  		<< comps[2] << " " << comps[3] << endl
	  		<< comps[4] << " " << comps[5] << endl;
	*/

  if (comps.size() != 6) {
  	cerr << "comps.size(): " << comps.size() << endl;
    throw runtime_error("Size of stat vector for ASRDevRate is not 6");
  }

  vector<ScoreStatsType> wer_hyp_ref_scores;
  vector<ScoreStatsType> wer_src_ref_scores;
  vector<ScoreStatsType> wer_hyp_src_scores;

  wer_hyp_ref_scores.push_back(comps[0]);
	wer_hyp_ref_scores.push_back(comps[1]);
	wer_src_ref_scores.push_back(comps[2]);
	wer_src_ref_scores.push_back(comps[3]);
	wer_hyp_src_scores.push_back(comps[4]);
	wer_hyp_src_scores.push_back(comps[5]);

  float wer_hyp_ref = m_wer_hyp_ref->calculateScore(wer_hyp_ref_scores);
  float wer_src_ref = m_wer_src_ref->calculateScore(wer_src_ref_scores);
  float wer_hyp_src = m_wer_hyp_src->calculateScore(wer_hyp_src_scores);

  return sqrt(wer_hyp_ref*wer_hyp_ref / 2.0 + pow(wer_src_ref - wer_hyp_src, 2) / 2.0);
}

}

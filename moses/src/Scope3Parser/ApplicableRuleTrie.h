/***********************************************************************
 Moses - statistical machine translation system
 Copyright (C) 2006-2012 University of Edinburgh

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

#pragma once

#include "Scope3Parser/SentenceMap.h"
#include "Scope3Parser/VarSpanNode.h"
#include "RuleTable/UTrieNode.h"
#include "Util.h"

#include <vector>

namespace Moses
{

struct VarSpanNode;

/** @todo what is this?
 */
struct ApplicableRuleTrie
{
 public:
  ApplicableRuleTrie(int start, int end, const UTrieNode &node)
      : m_start(start)
      , m_end(end)
      , m_node(&node)
      , m_vstNode(NULL) {}

  ~ApplicableRuleTrie() {
    RemoveAllInColl(m_children);
  }

  void Extend(const UTrieNode &root, int minPos, const SentenceMap &sentMap,
              bool followsGap);

  int m_start;
  int m_end;
  const UTrieNode *m_node;
  const VarSpanNode *m_vstNode;
  std::vector<ApplicableRuleTrie*> m_children;
};

}

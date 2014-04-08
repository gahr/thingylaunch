/*-
  Copyright (C) 2009-2014 Pietro Cerutti <gahr@gahr.ch>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.
  */

#include <fstream>
#include <iostream>
#include <iterator>

#include "history.h"
#include "util.h"

History::History()
    : m_historyFile { Util::getEnv("HOME") + "/.thingylaunch.history" }
{
    std::ifstream inFile { m_historyFile };
    std::string line;

    while (inFile.good()) {
        std::getline(inFile, line);
        if (!line.empty()) {
            m_elements.push_back(std::move(line));
        }
    }

    m_iter = m_elements.begin() - 1;
}

History::~History()
{
    // nothing to do...
}

std::string
History::next()
{
    if (m_elements.empty()) {
        return std::string();
    }

    if (m_iter >= m_elements.end()-1) {
        m_iter = m_elements.begin() - 1;
    }

    ++m_iter;

    return *m_iter;
}

std::string
History::prev()
{
    if (m_elements.empty()) {
        return std::string();
    }

    if (m_iter <= m_elements.begin()) {
        m_iter = m_elements.end();
    }

    --m_iter;

    return *m_iter;
}

void
History::save(std::string entry)
{
    std::ofstream outFile { m_historyFile };
    std::copy (m_elements.cbegin(), m_elements.cend(), 
            std::ostream_iterator<std::string>(outFile, "\n"));
    outFile << entry;
}

/*-
 * Copyright (C) Pietro Cerutti <gahr@gahr.ch>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <fstream>
#include <iostream>
#include <iterator>
using namespace std;

#include "history.h"
#include "util.h"

History::History()
    : m_historyFile { Util::getEnv("HOME") + "/.thingylaunch.history" }
{
    ifstream inFile { m_historyFile };
    string line;

    while (inFile.good()) {
        getline(inFile, line);
        if (!line.empty()) {
            m_elements.push_back(move(line));
        }
    }

    m_iter = begin(m_elements) - 1;
}

History::~History()
{
    // nothing to do...
}

string
History::next()
{
    if (m_elements.empty()) {
        return string();
    }

    if (m_iter >= end(m_elements) - 1) {
        m_iter = begin(m_elements) - 1;
    }

    ++m_iter;

    return *m_iter;
}

string
History::prev()
{
    if (m_elements.empty()) {
        return string();
    }

    if (m_iter <= begin(m_elements)) {
        m_iter = end(m_elements);
    }

    --m_iter;

    return *m_iter;
}

void
History::save(string entry)
{
    ofstream outFile { m_historyFile };
    copy (begin(m_elements), end(m_elements),
            ostream_iterator<string>(outFile, "\n"));
    if (!m_elements.size() || *(end(m_elements) - 1) != entry) {
        outFile << entry;
    }
}

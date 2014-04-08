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


#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>
#include <sstream>

#include "completion.h"

Completion::Completion()
{
    struct stat sb;
    uid_t uid { getuid() };
    gid_t gid { getgid() };

    /* get PATH env */
    const char * p { getenv("PATH") };
    if (p == nullptr) {
        throw std::runtime_error { "Could not find PATH environment variable." };
    }
    std::string path { p };

    /* tokenize path */
    std::string elem;
    std::stringstream ss { path };
    std::vector<std::string> pathElements;
    while (ss.good()) {
        getline(ss, elem, ':');
        pathElements.push_back(std::move(elem));
    }

    for (const auto& pathElem : pathElements) {

        /* open the directory pointed to by path */
        DIR * dirp { opendir(pathElem.c_str()) };
        if (dirp == nullptr) {
            continue;
        }

        /* traverse directory */
        struct dirent * dp;
        while ((dp = readdir(dirp))) {

            std::string currentPath { pathElem + "/" + dp->d_name };
            /* create a 'path/file' string and check whether we can access meta-information */
            if (stat(currentPath.c_str(), &sb) == -1) {
                continue;
            }

            /* a regular, executable file*/
            if (((sb.st_mode & S_IFREG) == S_IFREG) &&
                ((sb.st_uid == uid && (sb.st_mode & S_IXUSR) == S_IXUSR) ||
                 (sb.st_gid == gid && (sb.st_mode & S_IXGRP) == S_IXGRP) ||
                 ((sb.st_mode & S_IXOTH) == S_IXOTH)))
            {
                m_elements.push_back(dp->d_name);
            }
        }
        closedir(dirp);
    }

    /* initialize iterator to the begin */
    m_iter = m_elements.begin();
}

Completion::~Completion()
{
    // nothing to do...
}

std::string
Completion::next(std::string command)
{
    if (m_prefix.empty()) {
        m_prefix = command;
    }

    auto matchPrefix = [&command, this] (std::string e) { return e.compare(0, m_prefix.size(), this->m_prefix) == 0; };

    /* start from where we left off last time */
    auto iter1 = std::find_if(m_iter, m_elements.end(), matchPrefix);
    if (iter1 != m_elements.end()) {
        m_iter = iter1 + 1;
        return *iter1;
    }

    /* start over */
    auto iter2 = std::find_if(m_elements.begin(), m_elements.end(), matchPrefix);
    if (iter2 != m_elements.end()) {
        m_iter = iter2 + 1;
        return *iter2;
    }

    m_iter = m_elements.begin();

    return command;
}

void
Completion::reset()
{
    m_prefix.clear();
    m_iter = m_elements.begin();
}

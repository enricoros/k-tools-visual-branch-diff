/*
  Copyright (C) 2011, Enrico Ros <enrico.ros@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef GITSTRUCTURE_H
#define GITSTRUCTURE_H

#include <QString>
#include <QMap>
#include <QList>
#include <QByteArray>

namespace Git {

    typedef QString SHA1;

    /// represents the change (on a single code line?)
    struct Change {
        // Stats
        Git::SHA1 commitUid;
        QString shortUid;
        QString author;
        QString date;
        QString message;

        // relations
        //Change *nextChange; // NULL only if root of the current tree
        QList<Change *> precedingChanges; // NULL only if the first commit of a branch

        // temporary
        QList<Git::SHA1> unresolvedPreceding; // not empty only on incomplete subgraphs

        // default constructor
        Change();
        // copy constructor
        Change(const Change *p);
        // safe property manipulations
        void setNextChange(Change *item);
        // return "my_sha1...next_sha1"
        QString diffStringTo(const Change *next) const;
        // return "yr_sha1...my_sha1"
        QString diffStringFrom(const QString &sha1) const;
    };

    ///
    struct BranchHistory {
        Git::Change *lastChange;
        QList<const Git::Change *> changesFlatList;
        QMap<Git::SHA1, Git::Change *> idChangeMap;

        // [if not empty] all the nodes that have no parents in the tree
        QList<const Git::Change *> firstChanges;

        // [if not empty] all the nodes in the main path (the one that merges other's changes)
        QList<const Git::Change *> primaryPathChanges;

        // [if not empty] the diffs for each edge
        QMap<QString, QString> edgeDataMap;

        QStringList allEdgeDiffs(bool includeUnresolved) const;
        BranchHistory();
    };

    /// parses the output of "git log --parents" to create a flow of commits
    Git::BranchHistory parseLogToHistory(const QByteArray &log);

    /// subtracts B from A to find out what changed in the history
    Git::BranchHistory deltaHistory(const Git::BranchHistory &hA, const Git::BranchHistory &hB);

    /// parses the output of "git diff --stat" and builds a string with insertions and deletions
    QString parseDiffStat(const QByteArray &log);

} // namespace Git

#endif // GITSTRUCTURE_H

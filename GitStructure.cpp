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

#include "GitStructure.h"
#include <QTextStream>
#include <QStringList>

namespace Git {
//
// Change
//
Change::Change()
  //: nextChange(0)
{
}

Change::Change(const Change *p)
  : commitUid(p->commitUid), shortUid(p->shortUid), author(p->author)
  , date(p->date), message(p->message)//, nextChange(0)
{
}

void Change::setNextChange(Git::Change *item)
{
    if (item)
        item->precedingChanges.append(this);
    //if (nextChange)
    //    qWarning("already present!");
//    nextChange = item;
}

QString Change::diffStringTo(const Change *next) const
{
    return QString("%1...%2").arg(shortUid).arg(next->shortUid);
}

QString Change::diffStringFrom(const QString &sha1) const
{
    return QString("%1...%2").arg(sha1.left(8)).arg(shortUid);
}


//
// MergedHistory
//
QStringList BranchHistory::allEdgeDiffs(bool includeUnresolved) const
{
    QStringList edgeDiffs;
    foreach (const Git::Change *change, changesFlatList) {
        // all the diffs between change and its parents
        foreach (const Git::Change *parentChange, change->precedingChanges)
            edgeDiffs.append(parentChange->diffStringTo(change));
        // all the unresolved diffs
        if (includeUnresolved)
            foreach (const Git::SHA1 &parentChange, change->unresolvedPreceding)
                edgeDiffs.append(change->diffStringFrom(parentChange));
    }
    return edgeDiffs;
}

BranchHistory::BranchHistory()
  : lastChange(0)
{
}

//
// Utility functions
//
struct PostResolveMerge {
    Git::Change *owner;
    QList<Git::SHA1> parentIds;
};
typedef QList<PostResolveMerge> PostResolveMerges;

static void createLinks(const Git::PostResolveMerges &relations, Git::BranchHistory *history) {
    foreach (const Git::PostResolveMerge &prm, relations) {
        Git::Change *commit = prm.owner;
        foreach (const Git::SHA1 id, prm.parentIds) {
            if (!history->idChangeMap.contains(id)) {
                commit->unresolvedPreceding.append(id);
                //qWarning("createLinks: could not locate %s for merge", qPrintable(id.left(7)));
                continue;
            }
            Git::Change *parent = history->idChangeMap[id];
            parent->setNextChange(commit);
            history->firstChanges.removeAll(commit);
        }
    }
}

static void buildPrimaryPath(Git::BranchHistory *history)
{
    // build the primary path by descending through the first branch on every merge node
    history->primaryPathChanges.clear();
    if (!history->lastChange)
        return;
    Git::Change *change = history->lastChange;
    while (change) {
        history->primaryPathChanges.append(change);
        if (change->precedingChanges.isEmpty())
            break;
        change = change->precedingChanges.first();
    }
}

Git::BranchHistory parseLogToHistory(const QByteArray &log)
{
    /* Example commit
      commit 7a4d3c3a5889a3486eeb8d9bd61d64d669712b78 fc3566dd8afb671f5f2629103dc98fc790e21a90 5d642ece04e802dcbaa12629f75de3ea292e8444
      Merge: fc3566d 5d642ec
      Author: Cary Clark <cary@android.com>
      Date:   Thu Apr 22 06:51:03 2010 -0700
    */
    Git::BranchHistory history;

    Git::PostResolveMerges resolveRelations;
    QTextStream ts(log);
    while (!ts.atEnd()) {
        // sync to a start
        QString line = ts.readLine();
        if (!line.startsWith("commit "))
            continue;

        // create the MergeItem and add it to the History
        nextCommit:
        Git::Change *change = new Git::Change;
        QStringList ids = line.mid(7).split(" ", QString::SkipEmptyParts);
        change->commitUid = ids.takeFirst();
        change->shortUid = change->commitUid.left(8);

        history.idChangeMap[change->commitUid] = change;
        history.changesFlatList.append(change);
        history.firstChanges.append(change);
        if (!history.lastChange)
            history.lastChange = change;

        // post parent-child resolutions
        if (!ids.isEmpty()) {
            Git::PostResolveMerge resolve;
            resolve.owner = change;
            foreach (const Git::SHA1 &id, ids)
                resolve.parentIds.append(id);
            resolveRelations.append(resolve);
        }

        // parse log contents
        bool readingMessage = false;
        while (!ts.atEnd()) {
            line = ts.readLine();
            if (readingMessage) {
                if (line.startsWith("commit "))
                    break;
                change->message.append(line + "\n");
                continue;
            }
            if (line.startsWith("Author: ")) {
                change->author = line.mid(8);
                continue;
            }
            if (line.startsWith("Date: ")) {
                change->date = line.mid(6);
                if (!ts.atEnd()) {
                    if (!ts.readLine().isEmpty())
                        qWarning("parseLogToHistory: expected blank. ignoring.");
                }
                readingMessage = true;
                continue;
            }
            if (line.startsWith("Merge: ")) {
                //QStringList branches = line.mid(7).split(" ", QString::SkipEmptyParts);
                continue;
            }
        }

        // start parsing the next commit (TODO, make this cleaner)
        if (!ts.atEnd())
            goto nextCommit;
    }

    // post-resolution
    createLinks(resolveRelations, &history);

    // build the primary path
    buildPrimaryPath(&history);

    return history;
}

// result = A - B;
Git::BranchHistory deltaHistory(const Git::BranchHistory &hA, const Git::BranchHistory &hB)
{
    Git::BranchHistory history;

    QList<Git::PostResolveMerge> resolveRelations;
    foreach (const Git::Change *cA, hA.changesFlatList) {
        Git::SHA1 id = cA->commitUid;

        // skip change if present on the B history
        if (hB.idChangeMap.contains(id))
            continue;

        // duplicate the Change node
        Git::Change *change = new Git::Change(cA);
        history.idChangeMap[change->commitUid] = change;
        history.firstChanges.append(change);
        history.changesFlatList.append(change);
        if (!history.lastChange)
            history.lastChange = change;

        // add resolution link
        Git::PostResolveMerge resolve;
        resolve.owner = change;
        foreach (const Git::Change *parent, cA->precedingChanges)
            resolve.parentIds.append(parent->commitUid);
        resolveRelations.append(resolve);
    }

    // post-resolution
    createLinks(resolveRelations, &history);

    // build the primary path
    buildPrimaryPath(&history);

    return history;
}

QString parseDiffStat(const QByteArray &log)
{
    // if it's null, write this down
    if (log.isEmpty() || !log.contains("insertions"))
        return "=";

    // parse the information
    QTextStream ts(log);
    while (!ts.atEnd()) {
        QString line = ts.readLine();
        if (!line.contains("insertions") || !line.contains("deletions"))
            continue;
        // we have the right line
        QStringList tokens = line.split(",", QString::SkipEmptyParts);
        if (tokens.size() != 3) {
            qWarning("parseDiffStat: error validating git diff (3 tokens expected)");
            continue;
        }
        /*int files =*/ tokens.takeFirst().trimmed().split(" ").first().toInt();
        int inserts = tokens.takeFirst().trimmed().split(" ").first().toInt();
        int deletes = tokens.takeFirst().trimmed().split(" ").first().toInt();
        return QString("+%1 -%2").arg(inserts).arg(deletes);
    }
    return QString();
}

} // namespace Git

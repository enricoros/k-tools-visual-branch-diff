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

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "GitStructure.h"
#include "Console.h"
#include <QColorDialog>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QProcess>
#include <QSettings>
#include <QTextStream>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->runButton, SIGNAL(clicked()), this, SLOT(slotRunClicked()));
    connect(ui->locationPick, SIGNAL(clicked()), this, SLOT(slotPickLocation()));
    connect(ui->branch1Color, SIGNAL(clicked()), this, SLOT(slotPickColor()));
    connect(ui->branch2Color, SIGNAL(clicked()), this, SLOT(slotPickColor()));
    connect(ui->locationEdit, SIGNAL(textChanged(QString)), this, SLOT(populateBranchBoxes()));

    QSettings s;
    if (s.contains("General/LastPath"))
        ui->locationEdit->setText(s.value("General/LastPath").toString());

    setColor(0, Qt::darkGreen);
    setColor(1, Qt::blue);
    setProgress(-1);

    ui->statusBar->showMessage("Detected: " + Console::readCommandOutput(ui->locationEdit->text(), "git --version") +
                               " and: " + Console::readCommandOutput(ui->locationEdit->text(), "dot -V", 0, true).left(30));
}

MainWindow::~MainWindow()
{
    QSettings s;
    s.setValue("General/LastPath", ui->locationEdit->text());
    delete ui;
}

void MainWindow::setColor(int idx, const QColor &color)
{
    QPalette btPal;
    btPal.setColor(QPalette::ButtonText, color);
    QPalette tPal;
    tPal.setColor(QPalette::WindowText, color);
    QPalette bPal;
    bPal.setColor(QPalette::Button, color);
    switch (idx) {
    case 0:
        mColor1 = color;
        ui->branch1Label->setPalette(tPal);
        ui->branch1Combo->setPalette(btPal);
        ui->branch1Color->setPalette(bPal);
        break;
    case 1:
        mColor2 = color;
        ui->branch2Label->setPalette(tPal);
        ui->branch2Combo->setPalette(btPal);
        ui->branch2Color->setPalette(bPal);
        break;
    default:
        return;
    }
}

void MainWindow::setProgress(int value)
{
    ui->runProgress->setVisible(value >= 0);
    ui->runProgress->setValue(value);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void MainWindow::populateBranchBoxes()
{
    ui->branch1Combo->clear();
    ui->branch2Combo->clear();
    QStringList branches = QString(Console::readCommandOutput(ui->locationEdit->text(), "git branch -a")).split("\n");
    int index1 = -1, index2 = -1;
    foreach (const QString &branch, branches) {
        QString name = branch.trimmed();
        if (name.contains("->") || name.isEmpty())
            continue;
        if (name.startsWith("* "))
            name.remove(0, 2);
        if (name.startsWith("(no ", Qt::CaseInsensitive))
            continue;
        ui->branch1Combo->addItem(name);
        if (name.contains("froyo"))
            index1 = ui->branch1Combo->count() - 1;
        ui->branch2Combo->addItem(name);
        if (name.contains("gingerbread"))
            index2 = ui->branch2Combo->count() - 1;
    }
    if (index1 >= 0)
        ui->branch1Combo->setCurrentIndex(index1);
    if (index2 >= 0)
        ui->branch2Combo->setCurrentIndex(index2);
    if (ui->branch1Combo->count())
        ui->branch1Combo->insertItem(0, tr("The big bang"));

}

void MainWindow::slotPickColor()
{
    int colorIdx = sender() == ui->branch2Color;
    QColor color = QColorDialog::getColor(colorIdx ? mColor1 : mColor2, this, tr("Pick a color for this branch"));
    if (color.isValid())
        setColor(colorIdx, color);
}

void MainWindow::slotPickLocation()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Git Dir"), ui->locationEdit->text());
    if (dir.isEmpty() || !QFile::exists(dir))
        return;
    ui->locationEdit->setText(dir);
}

namespace Dot {

    static QString quoted(const QString &src)
    {
        return '"' + src + '"';
    }

    static void writeEdge(QTextStream &ts, const QString &from, const QString &to, const QString &label, const QString &attribs = QString())
    {
        ts << "    " << quoted(from) << " -> " << quoted(to);        
        ts << " [label=" << quoted(label);
        if (!attribs.isEmpty())
            ts << ", " << attribs;
        ts << "];" << "\n";
    }

    static void writeGraphFile(const Git::BranchHistory &history, const QString &mainLabel,
                               const QColor &color, const QColor &refColor, bool writeOnEdges,
                               const QString &outFileName)
    {
        // open the text stream
        QFile file(outFileName);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning("generateDotGraph: can't open output file '%s' for writing", qPrintable(outFileName));
            return;
        }
        QTextStream ts(&file);
        ts << "# This graph represents a Git history of " << history.changesFlatList.size() << " elements" << "\n";

        QString titleColor = quoted("#000080");

        QString nTextColor = quoted(color.name());
        QString nTextColorMerge = quoted(QColor(Qt::darkGray).name());
        QString nLineColor = quoted(QColor(Qt::black).name());
        QString nLineColorMerge = quoted(QColor(Qt::gray).name());

        QString lineColorRef = quoted(refColor.name());
        QString textColorRef = quoted(refColor.darker().name());

        QString eTextColor = quoted("#808080");
        QString eLineColor = quoted("#000000");
        QString eLineColorMerge = quoted("#800000");

        // header for a directed graph
        ts << "digraph graphname {" << "\n";

        // label
        if (!mainLabel.isEmpty()) {
            ts << "	   fontname=\"monospace\"; fontcolor=" << titleColor << "; fontsize=12;" << "\n";
            ts << "	   label=" + mainLabel + ";" << "\n";
        }

        // default looks

        ts << "    node [fontsize=8, color=" << nLineColor << ", fontcolor=" << nTextColor << ", shape=box, fontname=" + quoted("Courier 10 pitch") + "];" << "\n";
        ts << "    edge [fontsize=8, color=" << eLineColor << ", fontcolor=" << eTextColor << ", fontname=" + quoted("Arial") + "];" << "\n";

        // nodes
        ts << "    // nodes" << "\n";
        QStringList nodesMap;
        foreach (const Git::Change *item, history.changesFlatList) {
            // create label text
            QString label = item->message.split("\n", QString::SkipEmptyParts).first().simplified();
            label.replace("\"", "'");
            int i = 67;
            while (i < label.length()) {
                label.insert(i, "\\n");
                i += 68;
            }
            //label.replace(". ", ".\\n ");

            // add the node
            bool isMerge = (item->precedingChanges.size() + item->unresolvedPreceding.size()) > 1;
            QString attributes = "label=\"" + label + "\"";
            if (isMerge)
                attributes += ", shape=box, style=rounded, color=" + nLineColorMerge + ", fontcolor=" + nTextColorMerge;
            if (item->precedingChanges.isEmpty())
                attributes += ", color=" + nTextColor;
            ts << "    " << quoted(item->shortUid) << " [" << attributes << "];" << "\n";

            // add spare nodes for unresolved parents
            foreach (const Git::SHA1 &id, item->unresolvedPreceding) {
                QString name = id.left(7);
                if (!nodesMap.contains(name)) {
                    nodesMap.append(name);
                    ts << "    " << quoted(name) << " [shape=ellipse, color=" << lineColorRef << ", fontcolor=" << textColorRef <<  "];" << "\n";
                }
            }
        }

        // edges
        ts << "    // edges" << "\n";
        foreach (const Git::Change *change, history.changesFlatList) {
            // normal edges
            bool mergeLine = false;
            bool primaryItem = history.primaryPathChanges.contains(change);
            foreach (const Git::Change *precChange, change->precedingChanges) {
                QString edgeAttribs;
                if (!mergeLine && primaryItem)
                    edgeAttribs = "style=bold";
                if (mergeLine)
                    edgeAttribs = "color=" + eLineColorMerge;
                QString label;
                if (writeOnEdges) {
                    label = precChange->diffStringTo(change);
                    if (history.edgeDataMap.contains(label))
                        label += "  (" + history.edgeDataMap[label] + ")";
                }
                writeEdge(ts, precChange->shortUid, change->shortUid, label, edgeAttribs);
                mergeLine = true;
            }

            // unresolved commits edges
            foreach (const Git::SHA1 &precUnresolved, change->unresolvedPreceding) {
                QString label;
                if (writeOnEdges) {
                    label = change->diffStringFrom(precUnresolved);
                    if (history.edgeDataMap.contains(label))
                        label += "  (" + history.edgeDataMap[label] + ")";
                }
                writeEdge(ts, precUnresolved.left(7), change->shortUid, label, "style=dotted, color=" + lineColorRef);
            }
        }

        // tail
        ts << "}" << "\n";
    }

} // namespace Dot

void MainWindow::slotRunClicked()
{
    QString dir = ui->locationEdit->text();
    QString b1 = ui->branch1Combo->currentText();
    QString b2 = ui->branch2Combo->currentText();
    bool b1Off = !ui->branch1Combo->currentIndex();

    // verify branches
    QString branches = Console::readCommandOutput(dir, "git branch -a");
    if (!b1Off && !branches.contains(b1)) {
        ui->statusBar->showMessage("B1 ERROR");
        return;
    }
    if (!branches.contains(b2)) {
        ui->statusBar->showMessage("B2 ERROR");
        return;
    }
    setProgress(0);

    // get all the commits in branch 1
    Git::BranchHistory hist1;
    if (!b1Off) {
        QByteArray commits1 = Console::readCommandOutput(dir, "git log --parents " + b1);
         setProgress(5);
         hist1 = Git::parseLogToHistory(commits1);
         setProgress(10);
    }

    // get all the commits in branch 2
    QByteArray commits2 = Console::readCommandOutput(dir, "git log --parents " + b2);
     setProgress(15);
    Git::BranchHistory hist2 = Git::parseLogToHistory(commits2);
     setProgress(20);

    // delta = 2 - 1
    Git::BranchHistory histDelta = Git::deltaHistory(hist2, hist1);
     setProgress(30);

    // get all the diffs from the changes in the delta
    if (ui->showEdgeDiff->isChecked() && ui->showEdgeWeight->isChecked()) {
        QStringList edgesDiffs = histDelta.allEdgeDiffs(true);
        int edgeDiffsCount = edgesDiffs.size(), edgeDiffsIdx = 0;
        foreach (const QString &diff, edgesDiffs) {
            setProgress(30 + (50 * edgeDiffsIdx++) / edgeDiffsCount);
            bool ok = false;
            int duration = 0;
            QByteArray diffStat = Console::readCommandOutput(dir, "git diff --stat " + diff, &ok, false, &duration);
            if (!ok) {
                qWarning("error executing git diff %s", qPrintable(diff));
                continue;
            }
            QString edgeDiff = Git::parseDiffStat(diffStat);
            if (!edgeDiff.isEmpty())
                histDelta.edgeDataMap[diff] = edgeDiff;
            if (duration > 10) {
                qWarning("huge diff: %s [%s]", qPrintable(diff), qPrintable(edgeDiff));
            }
        }
        setProgress(80);
    }

    QString dotFileName = "/tmp/graph.dot";    
    QString label = tr("<<B>Graph of changes between</B>:<BR/><I>%1</I>, and<BR/><I>%2</I><BR/>(%3 new nodes)>")
            .arg(b1).arg(b2).arg(histDelta.changesFlatList.size());
    Dot::writeGraphFile(histDelta, label, mColor2, mColor1,
                        ui->showEdgeDiff->isChecked(), dotFileName);
     setProgress(85);

    QString dotType, dotExt;
    switch (ui->typesBox->currentIndex()) {
    case 0: dotType = "png"; dotExt = "png"; break;
    case 1: dotType = "svg"; dotExt = "svg"; break;
    case 2: dotType = "pdf"; dotExt = "pdf"; break;
    }

    QString targetFile = "/tmp/graph." + dotExt;
    QString genCommand = "dot -T" + dotType + " -Grankdir=BT -s0.5 -o" + targetFile + " " + dotFileName;
    QProcess::execute(genCommand);
     setProgress(95);

    ui->statusBar->showMessage(tr("Created file '%1' with %2 changes").arg(targetFile).arg(histDelta.idChangeMap.size()));

    QDesktopServices::openUrl(QUrl(targetFile));
    setProgress(100);
}

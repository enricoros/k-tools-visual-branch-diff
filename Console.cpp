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

#include "Console.h"
#include <QProcess>
#include <QTime>

QByteArray Console::readCommandOutput(const QString &dir, const QString &cmd, bool *ok, bool readError, int *duration)
{
    QProcess proc;
    proc.setWorkingDirectory(dir);
    if (readError)
        proc.setReadChannelMode(QProcess::MergedChannels);
    QTime timing;
    timing.start();
    proc.start(cmd);
    bool finished = proc.waitForFinished(60000);
    if (duration)
        *duration = qRound((qreal)timing.elapsed() / 1000.0);
    if (!finished && ok) {
        *ok = false;
        qWarning("Console::readCommandOutput: error %d (%s)", proc.error(), qPrintable(proc.errorString()));
        return QByteArray();
    }
    bool cleanExit = proc.exitCode() == 0;
    if (!cleanExit)
        qWarning("Console::readCommandOutput: unexpected return code: %d", proc.exitCode());
    if (ok)
        *ok = cleanExit;
    return proc.readAll();
}

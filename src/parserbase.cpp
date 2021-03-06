#include "parserbase.h"
#include <QDir>
#include <QMessageBox>
#include <QPushButton>
#include "accessmanager.h"
#include "danmakudelaygetter.h"
#include "downloader.h"
#include "platform/paths.h"
#include "playlist.h"
#include "reslibrary.h"
#include "selectiondialog.h"
#include "settings_network.h"
#include "parserykdl.h"
#include "parseryoutubedl.h"
#include "upgraderdialog.h"

#ifdef MP_ENABLE_WEBENGINE
#include "extractor.h"
#include "parserwebcatch.h"
#endif

QHash<QString,QString> saved_qualities;

ParserBase::ParserBase(QObject *parent) : QObject(parent)
{
}


ParserBase::~ParserBase()
{
}


void ParserBase::parse(const QString &url, bool download)
{
    this->url = url;
    this->download = download;
    result.title.clear();
    result.container.clear();
    result.danmaku_url.clear();
    result.urls.clear();
    result.referer.clear();
    result.ua.clear();
    result.seekable = true;
    result.is_dash = false;
    runParser(url);
}


void ParserBase::finishParsing()
{
    // Check if source is empty
    if (result.urls.isEmpty())
    {
        showErrorDialog(tr("The video's url is empty. Maybe it is a VIP video and requires login."));
        return;
    }

    // Bind referer and use-agent
    if (!result.referer.isEmpty())
    {
        foreach (QString url, result.urls)
            referer_table[QUrl(url).host()] = result.referer.toUtf8();
    }
    if (!result.ua.isEmpty())
    {
        foreach (QString url, result.urls)
            ua_table[QUrl(url).host()] = result.ua.toUtf8();
    }
    if (!result.seekable)
    {
         foreach (QString url, result.urls) {
            unseekable_hosts.append(QUrl(url).host());
        }
    }

    // replace illegal chars in title with .
    static QRegularExpression illegalChars("[\\\\/]");
    result.title.replace(illegalChars, ".");

    // Generate names list
    QStringList names;
    if (result.is_dash)
        names << ("video." + result.container) << ("audio." + result.container);
    else
    {
        for (int i = 0; i < result.urls.size(); i++)
            names << QString("%1_%2.%3").arg(result.title, QString::number(i).rightJustified(3, '0'), result.container);
    }

    // Download
    if (download)
    {
        // Build file path list
        QDir dir = QDir(Settings::downloadDir);
        QString dirname = result.title + '.' + result.container;
        if (result.urls.size() > 1)
        {
            if (!dir.cd(dirname))
            {
                dir.mkdir(dirname);
                dir.cd(dirname);
            }
        }
        for (int i = 0; i < names.size(); i++)
             names[i] = dir.filePath(QString(names[i]));

        // Download videos with danmaku
        if (!result.danmaku_url.isEmpty())
        {
            if (result.urls.size() > 1)
                new DanmakuDelayGetter(names, result.urls, result.danmaku_url, true, this);
            else
                downloader->addTask(result.urls[0].toUtf8(), names[0], false, result.danmaku_url.toUtf8());
        }
        // Download videos without danmaku
        else
        {
            for (int i = 0; i < result.urls.size(); i++)
                 downloader->addTask(result.urls[i].toUtf8(), names[i], result.urls.size() > 1);
        }
        QMessageBox::information(NULL, "Message", tr("Add download task successfully!"));
    }

    // Play
    else if (result.is_dash) // dash streams
    {
        playlist->addFileAndPlay(result.title, result.urls[0], 0, result.urls[1]);
        res_library->close();
    }
    else if (!result.danmaku_url.isEmpty()) // with danmaku
    {
        if (result.urls.size() > 1)
            new DanmakuDelayGetter(names, result.urls, result.danmaku_url, false, this);
        else
            playlist->addFileAndPlay(names[0], result.urls[0], result.danmaku_url);
        res_library->close();
    }
    else
    {
        playlist->addFileAndPlay(names[0], result.urls[0]);
        for (int i = 1; i < result.urls.size(); i++)
            playlist->addFile(names[i], result.urls[i]);
        res_library->close();
    }
}

int ParserBase::selectQuality(const QStringList &stream_types)
{
    static SelectionDialog *selectionDialog = NULL;
    if (selectionDialog == NULL)
        selectionDialog = new SelectionDialog;

    // If there is only one quality, use it
    if (stream_types.length() == 1)
        return 0;

    // Check if the selection is already saved
    QString host = QUrl(url).host();
    if (saved_qualities.contains(host))
    {
        QString quality = saved_qualities[host];

        // Check if the saved quality is available in this video
        for (int i = 0; i < stream_types.length(); i++)
        {
            if (stream_types[i] == quality)
                return i;
        }
    }

    // Not saved, show dialog
    bool remember = false;
    int selected = selectionDialog->showDialog_Index(stream_types,
                                                     tr("Please select a video quality:"),
                                                     tr("Remember my selection for this website"),
                                                     &remember);
    if (remember && selected != -1) // save selection
        saved_qualities[host] = stream_types[selected];
    return selected;
}

void ParserBase::showErrorDialog(const QString &errMsg)
{
    QMessageBox msgBox;
    msgBox.setText("Error");
    msgBox.setInformativeText("Parse failed!\nURL:" + url);
    msgBox.setDetailedText("URL: " + url + "\n\n" + errMsg);
    QPushButton *updateButton = msgBox.addButton(tr("Upgrade parser"), QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Cancel);
    msgBox.exec();
    if (msgBox.clickedButton() == updateButton)
        upgraderDialog->runUpgrader();
}


void parseUrl(const QString &url, bool download)
{
    QString host = QUrl(url).host();
#ifdef MP_ENABLE_WEBENGINE
    if (Extractor::isSupported(host))
        parser_webcatch->parse(url, download);
    else
#endif
    if (ParserYkdl::isSupported(host))
        parser_ykdl->parse(url, download);
    else
        parser_youtubedl->parse(url, download);
}


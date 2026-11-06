#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "theme.h"
#include "translator.h"

class SpotTests : public QObject {
    Q_OBJECT

private slots:
    void parseShazamJsonSuccess()
    {
        const QByteArray sample =
            "{\n"
            "  \"matches\": [{\"id\": \"12345\"}],\n"
            "  \"track\": {\n"
            "    \"title\": \"Starman\",\n"
            "    \"subtitle\": \"David Bowie\",\n"
            "    \"images\": {\n"
            "      \"coverart\": \"https://example.com/starman.jpg\"\n"
            "    },\n"
            "    \"sections\": [\n"
            "      {\n"
            "        \"type\": \"SONG\",\n"
            "        \"metadata\": [\n"
            "          {\"title\": \"Album\", \"text\": \"The Rise and Fall of Ziggy Stardust\"},\n"
            "          {\"title\": \"Released\", \"text\": \"1972\"}\n"
            "        ]\n"
            "      }\n"
            "    ],\n"
            "    \"url\": \"https://www.shazam.com/track/12345\"\n"
            "  }\n"
            "}";

        TrackInfo info = TrackInfo::fromShazamJson(sample);
        QCOMPARE(info.title, QString("Starman"));
        QCOMPARE(info.artist, QString("David Bowie"));
        QCOMPARE(info.album, QString("The Rise and Fall of Ziggy Stardust"));
        QCOMPARE(info.year, QString("1972"));
        QCOMPARE(info.coverUrl, QString("https://example.com/starman.jpg"));
        QCOMPARE(info.shazamUrl, QString("https://www.shazam.com/track/12345"));
        QVERIFY(info.spotifyUrl.contains("Starman"));
    }

    void parseShazamJsonNoMatch()
    {
        const QByteArray empty = "{\"matches\": []}";
        TrackInfo info = TrackInfo::fromShazamJson(empty);
        QVERIFY(info.title.isEmpty());
        QVERIFY(info.artist.isEmpty());
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        auto write = [](const QString &path, const QByteArray &data) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        QVERIFY(write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n"));
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write(response, "{\"int\":0}"));

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        QVERIFY(write(response, "{\"int\":6}"));
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "da\n");
        write(dir.path() + "/da.json", "{\"Connect Spotify\": \"Forbind Spotify\", \"Close\": \"Luk\"}");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("da"));
        QCOMPARE(translator.translate("Main", "Connect Spotify"), QString("Forbind Spotify"));
        QCOMPARE(translator.translate("Main", "Close"), QString("Luk"));
        QVERIFY(translator.translate("Main", "Unknown").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "da\n");
        write(dir.path() + "/en.json", "{\"%n songs\": {\"one\": \"%n song\", \"other\": \"%n songs\"}}");
        write(dir.path() + "/da.json", "{\"%n songs\": {\"one\": \"%n sang\", \"other\": \"%n sange\"}}");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n songs", nullptr, 1), QString("%n sang"));
        QCOMPARE(translator.translate("Main", "%n songs", nullptr, 10), QString("%n sange"));
    }

    void renderMainQmlOffscreen()
    {
        Theme theme;
        Backend backend;
        Translator translator;

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("translator", &translator);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());

        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->show();
        QImage image = window->grabWindow();
        QVERIFY(!image.isNull());
        image.save("/tmp/spot_window_preview.png");
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(SpotTests)
#include "spot_tests.moc"

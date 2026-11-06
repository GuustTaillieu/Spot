#include "backend.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QClipboard>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QUrlQuery>

TrackInfo TrackInfo::fromShazamJson(const QByteArray &json) {
    TrackInfo info;
    const auto doc = QJsonDocument::fromJson(json);
    if (!doc.isObject())
        return info;

    const auto root = doc.object();
    if (!root.contains("track") || !root.value("track").isObject())
        return info;

    const auto track = root.value("track").toObject();
    info.title = track.value("title").toString();
    info.artist = track.value("subtitle").toString();

    // Artwork
    const auto images = track.value("images").toObject();
    if (images.contains("coverart"))
        info.coverUrl = images.value("coverart").toString();
    else if (images.contains("default"))
        info.coverUrl = images.value("default").toString();
    else if (images.contains("background"))
        info.coverUrl = images.value("background").toString();

    // Sections for album and release date
    const auto sections = track.value("sections").toArray();
    for (const auto &secVal : sections) {
        const auto secObj = secVal.toObject();
        if (secObj.value("type").toString() == "SONG") {
            const auto metadata = secObj.value("metadata").toArray();
            for (const auto &metaVal : metadata) {
                const auto m = metaVal.toObject();
                const auto title = m.value("title").toString();
                if (title.compare("Album", Qt::CaseInsensitive) == 0)
                    info.album = m.value("text").toString();
                else if (title.compare("Released", Qt::CaseInsensitive) == 0)
                    info.year = m.value("text").toString();
            }
        }
    }

    // Shazam URL
    if (track.contains("url"))
        info.shazamUrl = track.value("url").toString();
    else if (track.contains("share") && track.value("share").toObject().contains("href"))
        info.shazamUrl = track.value("share").toObject().value("href").toString();

    // Default Spotify search fallback URL
    const QString query = QUrl::toPercentEncoding(info.title + " " + info.artist);
    info.spotifyUrl = "https://open.spotify.com/search/" + query;
    info.timestamp = QDateTime::currentDateTime();

    return info;
}

QVariantMap TrackInfo::toMap() const {
    QVariantMap map;
    map["title"] = title;
    map["artist"] = artist;
    map["album"] = album;
    map["year"] = year;
    map["coverUrl"] = coverUrl;
    map["shazamUrl"] = shazamUrl;
    map["spotifyId"] = spotifyId;
    map["spotifyUrl"] = spotifyUrl;
    map["spotifyUri"] = spotifyUri;
    map["timestamp"] = timestamp.toString(Qt::ISODate);
    return map;
}

TrackInfo TrackInfo::fromMap(const QVariantMap &map) {
    TrackInfo info;
    info.title = map.value("title").toString();
    info.artist = map.value("artist").toString();
    info.album = map.value("album").toString();
    info.year = map.value("year").toString();
    info.coverUrl = map.value("coverUrl").toString();
    info.shazamUrl = map.value("shazamUrl").toString();
    info.spotifyId = map.value("spotifyId").toString();
    info.spotifyUrl = map.value("spotifyUrl").toString();
    info.spotifyUri = map.value("spotifyUri").toString();
    info.timestamp = QDateTime::fromString(map.value("timestamp").toString(), Qt::ISODate);
    return info;
}

Backend::Backend(QObject *parent) : QObject(parent) {
    connect(&m_recordingProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Backend::onFfmpegFinished);
    connect(&m_songrecProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Backend::onSongrecFinished);
    connect(&m_authServer, &QTcpServer::newConnection,
            this, &Backend::onAuthServerNewConnection);

    refreshAudioDevices();
    loadSettings();
    loadHistory();

    if (m_spotifyConnected && !m_spotifyAccessToken.isEmpty()) {
        fetchSpotifyUserProfile();
        fetchSpotifyPlaylists();
    }
}

Backend::~Backend() {
    if (m_recordingProcess.state() != QProcess::NotRunning) {
        m_recordingProcess.kill();
        m_recordingProcess.waitForFinished(500);
    }
    if (m_songrecProcess.state() != QProcess::NotRunning) {
        m_songrecProcess.kill();
        m_songrecProcess.waitForFinished(500);
    }
    if (m_authServer.isListening()) {
        m_authServer.close();
    }
}

void Backend::setStatusText(const QString &text) {
    if (m_statusText != text) {
        m_statusText = text;
        emit statusTextChanged();
    }
}

void Backend::setSelectedDeviceIndex(int index) {
    if (index >= 0 && index < m_audioDevices.size() && m_selectedDeviceIndex != index) {
        m_selectedDeviceIndex = index;
        emit selectedDeviceIndexChanged();
    }
}

void Backend::setSpotifyClientId(const QString &id) {
    const QString clean = id.trimmed();
    if (m_spotifyClientId != clean) {
        m_spotifyClientId = clean;
        emit spotifyClientIdChanged();
        saveSettings();
    }
}

void Backend::refreshAudioDevices() {
    QProcess process;
    process.start("pactl", {"list", "sources"});
    if (!process.waitForFinished(2000)) {
        // Fallback default
        QVariantMap def;
        def["id"] = "default";
        def["name"] = tr("Default Audio Input");
        def["isMonitor"] = false;
        m_audioDevices = {def};
        emit audioDevicesChanged();
        return;
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    const QStringList lines = output.split('\n');
    QVariantList devices;

    QString currentName;
    QString currentDesc;

    auto appendCurrent = [&]() {
        if (!currentName.isEmpty()) {
            QVariantMap item;
            item["id"] = currentName;
            item["name"] = currentDesc.isEmpty() ? currentName : currentDesc;
            item["isMonitor"] = currentName.contains(".monitor");
            devices.append(item);
            currentName.clear();
            currentDesc.clear();
        }
    };

    for (const auto &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith("Name: ")) {
            appendCurrent();
            currentName = trimmed.mid(6).trimmed();
        } else if (trimmed.startsWith("Description: ")) {
            currentDesc = trimmed.mid(13).trimmed();
        }
    }
    appendCurrent();

    if (devices.isEmpty()) {
        QVariantMap def;
        def["id"] = "default";
        def["name"] = tr("Default Microphone");
        def["isMonitor"] = false;
        devices.append(def);
    }

    m_audioDevices = devices;
    if (m_selectedDeviceIndex >= m_audioDevices.size()) {
        m_selectedDeviceIndex = 0;
        emit selectedDeviceIndexChanged();
    }
    emit audioDevicesChanged();
}

void Backend::primaryAction() {
    startListening();
}

void Backend::startListening() {
    if (m_isListening) {
        cancelListening();
        return;
    }

    QString deviceId = "default";
    if (m_selectedDeviceIndex >= 0 && m_selectedDeviceIndex < m_audioDevices.size()) {
        deviceId = m_audioDevices.at(m_selectedDeviceIndex).toMap().value("id").toString();
    }

    m_isListening = true;
    emit isListeningChanged();
    setStatusText(tr("Listening to %1...").arg(m_audioDevices.at(m_selectedDeviceIndex).toMap().value("name").toString()));

    // Record 5 seconds of 44.1kHz mono PCM audio
    QStringList args;
    args << "-y" << "-f" << "pulse" << "-i" << deviceId << "-t" << "5"
         << "-ac" << "1" << "-ar" << "44100" << m_tempWavPath;

    m_recordingProcess.start("ffmpeg", args);
}

void Backend::cancelListening() {
    if (m_recordingProcess.state() != QProcess::NotRunning) {
        m_recordingProcess.kill();
    }
    if (m_songrecProcess.state() != QProcess::NotRunning) {
        m_songrecProcess.kill();
    }
    m_isListening = false;
    emit isListeningChanged();
    setStatusText(tr("Ready"));
}

void Backend::onFfmpegFinished(int exitCode, QProcess::ExitStatus) {
    if (!m_isListening)
        return;

    if (exitCode != 0 || !QFile::exists(m_tempWavPath)) {
        m_isListening = false;
        emit isListeningChanged();
        setStatusText(tr("Audio recording failed. Check microphone permissions."));
        return;
    }

    setStatusText(tr("Identifying song with Shazam..."));
    m_songrecProcess.start("songrec", {"audio-file-to-recognized-song", m_tempWavPath});
}

void Backend::onSongrecFinished(int exitCode, QProcess::ExitStatus) {
    m_isListening = false;
    emit isListeningChanged();

    if (exitCode != 0) {
        setStatusText(tr("Recognition error occurred."));
        return;
    }

    const QByteArray output = m_songrecProcess.readAllStandardOutput();
    TrackInfo recognized = TrackInfo::fromShazamJson(output);

    if (recognized.title.isEmpty()) {
        setStatusText(tr("No match found. Try again closer to the speaker."));
        return;
    }

    m_currentTrack = recognized;
    m_hasTrack = true;
    m_isTrackLiked = false;
    m_trackPlaylistIds.clear();
    emit trackPlaylistsChanged();
    emit trackChanged();

    setStatusText(tr("Found: %1 — %2").arg(recognized.title, recognized.artist));
    addToHistory(recognized);

    if (m_spotifyConnected) {
        searchSpotifyTrack(recognized.title, recognized.artist);
    }
}

void Backend::openSpotify() {
    if (!m_currentTrack.spotifyUrl.isEmpty()) {
        QDesktopServices::openUrl(QUrl(m_currentTrack.spotifyUrl));
    }
}

void Backend::copySpotifyLink() {
    if (!m_currentTrack.spotifyUrl.isEmpty()) {
        QGuiApplication::clipboard()->setText(m_currentTrack.spotifyUrl);
        setStatusText(tr("Spotify link copied to clipboard!"));
    }
}

void Backend::openShazam() {
    if (!m_currentTrack.shazamUrl.isEmpty()) {
        QDesktopServices::openUrl(QUrl(m_currentTrack.shazamUrl));
    }
}

void Backend::addToHistory(const TrackInfo &track) {
    QVariantMap map = track.toMap();
    m_history.prepend(map);
    while (m_history.size() > 50)
        m_history.removeLast();
    emit historyChanged();
    saveHistory();
}

void Backend::selectHistoryTrack(int index) {
    if (index >= 0 && index < m_history.size()) {
        m_currentTrack = TrackInfo::fromMap(m_history.at(index).toMap());
        m_hasTrack = true;
        m_isTrackLiked = false;
        m_trackPlaylistIds.clear();
        emit trackPlaylistsChanged();
        emit trackChanged();
        setStatusText(tr("Loaded %1 — %2").arg(m_currentTrack.title, m_currentTrack.artist));

        if (m_spotifyConnected) {
            if (m_currentTrack.spotifyId.isEmpty())
                searchSpotifyTrack(m_currentTrack.title, m_currentTrack.artist);
            else
                checkTrackPlaylists();
        }
    }
}

void Backend::clearHistory() {
    m_history.clear();
    emit historyChanged();
    saveHistory();
}

void Backend::loadSettings() {
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/spot";
    QFile file(configDir + "/spotify.json");
    if (!file.open(QIODevice::ReadOnly))
        return;

    const auto obj = QJsonDocument::fromJson(file.readAll()).object();
    if (obj.contains("clientId") && !obj.value("clientId").toString().isEmpty())
        m_spotifyClientId = obj.value("clientId").toString();
    m_spotifyAccessToken = obj.value("accessToken").toString();
    m_spotifyRefreshToken = obj.value("refreshToken").toString();
    m_spotifyUser = obj.value("userName").toString();
    if (obj.contains("expiresAt"))
        m_spotifyTokenExpiresAt = QDateTime::fromString(obj.value("expiresAt").toString(), Qt::ISODate);

    m_spotifyConnected = !m_spotifyAccessToken.isEmpty();
    emit spotifyConnectedChanged();
    emit spotifyUserChanged();
    emit spotifyClientIdChanged();
}

void Backend::saveSettings() {
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/spot";
    QDir().mkpath(configDir);
    QFile file(configDir + "/spotify.json");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QJsonObject obj;
    obj["clientId"] = m_spotifyClientId;
    obj["accessToken"] = m_spotifyAccessToken;
    obj["refreshToken"] = m_spotifyRefreshToken;
    obj["userName"] = m_spotifyUser;
    obj["expiresAt"] = m_spotifyTokenExpiresAt.toString(Qt::ISODate);
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

void Backend::loadHistory() {
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/spot";
    QFile file(dataDir + "/history.json");
    if (!file.open(QIODevice::ReadOnly))
        return;

    m_history = QJsonDocument::fromJson(file.readAll()).array().toVariantList();
    emit historyChanged();
}

void Backend::saveHistory() {
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/spot";
    QDir().mkpath(dataDir);
    QFile file(dataDir + "/history.json");
    if (!file.open(QIODevice::WriteOnly))
        return;

    file.write(QJsonDocument(QJsonArray::fromVariantList(m_history)).toJson(QJsonDocument::Indented));
}

// Spotify OAuth PKCE Flow
void Backend::startSpotifyAuth() {
    if (m_spotifyClientId.isEmpty()) {
        setStatusText(tr("Please provide a Spotify Client ID first."));
        return;
    }

    if (m_authServer.isListening())
        m_authServer.close();

    if (!m_authServer.listen(QHostAddress::LocalHost, 8888)) {
        setStatusText(tr("Could not bind port 8888 for Spotify callback."));
        return;
    }

    // Generate PKCE code verifier and challenge
    static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~";
    m_pkceVerifier.clear();
    for (int i = 0; i < 64; ++i) {
        m_pkceVerifier.append(chars[QRandomGenerator::global()->bounded(int(sizeof(chars) - 1))]);
    }

    const QByteArray hash = QCryptographicHash::hash(m_pkceVerifier.toUtf8(), QCryptographicHash::Sha256);
    QString challenge = QString::fromLatin1(hash.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));

    QUrl url("https://accounts.spotify.com/authorize");
    QUrlQuery query;
    query.addQueryItem("client_id", m_spotifyClientId);
    query.addQueryItem("response_type", "code");
    query.addQueryItem("redirect_uri", "http://127.0.0.1:8888/callback");
    query.addQueryItem("code_challenge_method", "S256");
    query.addQueryItem("code_challenge", challenge);
    query.addQueryItem("scope", "playlist-read-private playlist-read-collaborative playlist-modify-public playlist-modify-private user-library-read user-library-modify");
    url.setQuery(query);

    setStatusText(tr("Opening browser for Spotify authorization..."));
    QDesktopServices::openUrl(url);
}

void Backend::onAuthServerNewConnection() {
    QTcpSocket *socket = m_authServer.nextPendingConnection();
    if (!socket)
        return;

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        const QByteArray req = socket->readAll();
        const QString line = QString::fromUtf8(req).section("\r\n", 0, 0);
        const QString path = line.section(' ', 1, 1);

        QUrl url("http://127.0.0.1:8888" + path);
        QUrlQuery query(url.query());
        const QString code = query.queryItemValue("code");

        const QString response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Connection: close\r\n\r\n"
            "<!DOCTYPE html><html><body style='background:#121212;color:#fff;font-family:sans-serif;text-align:center;padding:50px;'>"
            "<h1>Spot &times; Spotify Connected!</h1>"
            "<p>You can close this tab and return to the Spot app.</p>"
            "</body></html>";

        socket->write(response.toUtf8());
        socket->flush();
        socket->disconnectFromHost();
        m_authServer.close();

        if (code.isEmpty()) {
            setStatusText(tr("Spotify authorization was cancelled or failed."));
            return;
        }

        // Exchange code for tokens
        QNetworkRequest tokenReq(QUrl("https://accounts.spotify.com/api/token"));
        tokenReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        QUrlQuery body;
        body.addQueryItem("grant_type", "authorization_code");
        body.addQueryItem("code", code);
        body.addQueryItem("redirect_uri", "http://127.0.0.1:8888/callback");
        body.addQueryItem("client_id", m_spotifyClientId);
        body.addQueryItem("code_verifier", m_pkceVerifier);

        QNetworkReply *reply = m_network.post(tokenReq, body.toString(QUrl::FullyEncoded).toUtf8());
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                setStatusText(tr("Token exchange failed: %1").arg(reply->errorString()));
                return;
            }

            const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
            m_spotifyAccessToken = obj.value("access_token").toString();
            m_spotifyRefreshToken = obj.value("refresh_token").toString();
            const int expiresIn = obj.value("expires_in").toInt(3600);
            m_spotifyTokenExpiresAt = QDateTime::currentDateTime().addSecs(expiresIn - 60);

            m_spotifyConnected = true;
            emit spotifyConnectedChanged();
            setStatusText(tr("Connected to Spotify!"));

            fetchSpotifyUserProfile();
            fetchSpotifyPlaylists();
            saveSettings();
        });
    });
}

void Backend::disconnectSpotify() {
    m_spotifyConnected = false;
    m_spotifyAccessToken.clear();
    m_spotifyRefreshToken.clear();
    m_spotifyUser.clear();
    m_playlists.clear();
    m_trackPlaylistIds.clear();
    m_isTrackLiked = false;
    emit spotifyConnectedChanged();
    emit spotifyUserChanged();
    emit playlistsChanged();
    emit trackPlaylistsChanged();
    saveSettings();
    setStatusText(tr("Disconnected from Spotify."));
}

void Backend::refreshSpotifyToken() {
    if (m_spotifyRefreshToken.isEmpty() || m_spotifyClientId.isEmpty())
        return;

    QNetworkRequest req(QUrl("https://accounts.spotify.com/api/token"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery body;
    body.addQueryItem("grant_type", "refresh_token");
    body.addQueryItem("refresh_token", m_spotifyRefreshToken);
    body.addQueryItem("client_id", m_spotifyClientId);

    QNetworkReply *reply = m_network.post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
            m_spotifyAccessToken = obj.value("access_token").toString();
            if (obj.contains("refresh_token"))
                m_spotifyRefreshToken = obj.value("refresh_token").toString();
            const int expiresIn = obj.value("expires_in").toInt(3600);
            m_spotifyTokenExpiresAt = QDateTime::currentDateTime().addSecs(expiresIn - 60);
            saveSettings();
        }
    });
}

void Backend::fetchSpotifyUserProfile() {
    if (!m_spotifyConnected || m_spotifyAccessToken.isEmpty())
        return;

    QNetworkRequest req(QUrl("https://api.spotify.com/v1/me"));
    req.setRawHeader("Authorization", "Bearer " + m_spotifyAccessToken.toUtf8());

    QNetworkReply *reply = m_network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
            m_spotifyUser = obj.value("display_name").toString();
            emit spotifyUserChanged();
            saveSettings();
        }
    });
}

void Backend::fetchSpotifyPlaylists() {
    if (!m_spotifyConnected || m_spotifyAccessToken.isEmpty())
        return;

    QNetworkRequest req(QUrl("https://api.spotify.com/v1/me/playlists?limit=50"));
    req.setRawHeader("Authorization", "Bearer " + m_spotifyAccessToken.toUtf8());

    QNetworkReply *reply = m_network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            const auto items = QJsonDocument::fromJson(reply->readAll()).object().value("items").toArray();
            QVariantList list;
            for (const auto &it : items) {
                const auto pl = it.toObject();
                QVariantMap map;
                map["id"] = pl.value("id").toString();
                map["name"] = pl.value("name").toString();

                int count = 0;
                if (pl.contains("items") && pl.value("items").isObject()) {
                    count = pl.value("items").toObject().value("total").toInt();
                } else if (pl.contains("tracks") && pl.value("tracks").isObject()) {
                    count = pl.value("tracks").toObject().value("total").toInt();
                }
                map["tracksCount"] = count;

                QString imgUrl;
                const auto images = pl.value("images").toArray();
                if (!images.isEmpty())
                    imgUrl = images.first().toObject().value("url").toString();
                map["imageUrl"] = imgUrl;

                list.append(map);
            }
            m_playlists = list;
            emit playlistsChanged();

            if (m_hasTrack && !m_currentTrack.spotifyId.isEmpty())
                checkTrackPlaylists();
        }
    });
}

void Backend::refreshPlaylists() {
    fetchSpotifyPlaylists();
}

void Backend::searchSpotifyTrack(const QString &title, const QString &artist) {
    if (!m_spotifyConnected || m_spotifyAccessToken.isEmpty())
        return;

    auto cleanText = [](const QString &s) {
        QString clean = s;
        clean.remove(QRegularExpression(R"(\s*[\(\[].*?[\)\]])"));
        clean.remove(QRegularExpression(R"(\s*-\s*.*)"));
        return clean.trimmed();
    };

    const QString cleanTitle = cleanText(title);
    const QString cleanArtist = cleanText(artist);

    const QString q = QString("track:%1 artist:%2").arg(cleanTitle.isEmpty() ? title : cleanTitle,
                                                        cleanArtist.isEmpty() ? artist : cleanArtist);
    QUrl url("https://api.spotify.com/v1/search");
    QUrlQuery query;
    query.addQueryItem("q", q);
    query.addQueryItem("type", "track");
    query.addQueryItem("limit", "1");
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_spotifyAccessToken.toUtf8());

    QNetworkReply *reply = m_network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, title, artist]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;

        const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
        const auto items = obj.value("tracks").toObject().value("items").toArray();
        if (items.isEmpty()) {
            // Broader search fallback
            QUrl broadUrl("https://api.spotify.com/v1/search");
            QUrlQuery broadQuery;
            broadQuery.addQueryItem("q", title + " " + artist);
            broadQuery.addQueryItem("type", "track");
            broadQuery.addQueryItem("limit", "1");
            broadUrl.setQuery(broadQuery);

            QNetworkRequest broadReq(broadUrl);
            broadReq.setRawHeader("Authorization", "Bearer " + m_spotifyAccessToken.toUtf8());
            QNetworkReply *broadReply = m_network.get(broadReq);
            connect(broadReply, &QNetworkReply::finished, this, [this, broadReply]() {
                broadReply->deleteLater();
                if (broadReply->error() != QNetworkReply::NoError)
                    return;
                const auto bObj = QJsonDocument::fromJson(broadReply->readAll()).object();
                const auto bItems = bObj.value("tracks").toObject().value("items").toArray();
                if (!bItems.isEmpty()) {
                    const auto trk = bItems.first().toObject();
                    m_currentTrack.spotifyId = trk.value("id").toString();
                    m_currentTrack.spotifyUri = trk.value("uri").toString();
                    m_currentTrack.spotifyUrl = trk.value("external_urls").toObject().value("spotify").toString();

                    const auto alb = trk.value("album").toObject();
                    if (m_currentTrack.album.isEmpty())
                        m_currentTrack.album = alb.value("name").toString();
                    const auto imgs = alb.value("images").toArray();
                    if (!imgs.isEmpty() && m_currentTrack.coverUrl.isEmpty())
                        m_currentTrack.coverUrl = imgs.first().toObject().value("url").toString();

                    emit trackChanged();
                    checkTrackPlaylists();
                }
            });
            return;
        }

        const auto track = items.first().toObject();
        m_currentTrack.spotifyId = track.value("id").toString();
        m_currentTrack.spotifyUri = track.value("uri").toString();
        m_currentTrack.spotifyUrl = track.value("external_urls").toObject().value("spotify").toString();

        const auto album = track.value("album").toObject();
        if (m_currentTrack.album.isEmpty())
            m_currentTrack.album = album.value("name").toString();

        const auto images = album.value("images").toArray();
        if (!images.isEmpty() && m_currentTrack.coverUrl.isEmpty())
            m_currentTrack.coverUrl = images.first().toObject().value("url").toString();

        emit trackChanged();
        checkTrackPlaylists();
    });
}

void Backend::checkTrackPlaylists() {
    if (!m_spotifyConnected || m_spotifyAccessToken.isEmpty() || m_currentTrack.spotifyId.isEmpty())
        return;

    const QString trackUri = m_currentTrack.spotifyUri.isEmpty()
                           ? QString("spotify:track:%1").arg(m_currentTrack.spotifyId)
                           : m_currentTrack.spotifyUri;

    // 1. Check if track is in Library via /v1/me/library/contains?uris=...
    QUrl likedUrl("https://api.spotify.com/v1/me/library/contains");
    QUrlQuery q;
    q.addQueryItem("uris", trackUri);
    likedUrl.setQuery(q);

    QNetworkRequest req(likedUrl);
    req.setRawHeader("Authorization", "Bearer " + m_spotifyAccessToken.toUtf8());

    QNetworkReply *reply = m_network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            const auto arr = QJsonDocument::fromJson(reply->readAll()).array();
            if (!arr.isEmpty()) {
                m_isTrackLiked = arr.first().toBool();
                emit trackPlaylistsChanged();
            }
        }
    });

    // 2. Check each playlist if it contains this track via /v1/playlists/{id}/items
    for (const auto &plVar : m_playlists) {
        const QString plId = plVar.toMap().value("id").toString();
        QUrl plUrl(QString("https://api.spotify.com/v1/playlists/%1/items").arg(plId));
        QUrlQuery plQuery;
        plQuery.addQueryItem("limit", "100");
        plUrl.setQuery(plQuery);

        QNetworkRequest plReq(plUrl);
        plReq.setRawHeader("Authorization", "Bearer " + m_spotifyAccessToken.toUtf8());

        QNetworkReply *plReply = m_network.get(plReq);
        connect(plReply, &QNetworkReply::finished, this, [this, plReply, plId]() {
            plReply->deleteLater();
            if (plReply->error() == QNetworkReply::NoError) {
                const auto items = QJsonDocument::fromJson(plReply->readAll()).object().value("items").toArray();
                bool found = false;
                for (const auto &it : items) {
                    const auto obj = it.toObject();
                    QString trkId;
                    if (obj.contains("item") && obj.value("item").isObject())
                        trkId = obj.value("item").toObject().value("id").toString();
                    else if (obj.contains("track") && obj.value("track").isObject())
                        trkId = obj.value("track").toObject().value("id").toString();

                    if (trkId == m_currentTrack.spotifyId) {
                        found = true;
                        break;
                    }
                }
                if (found && !m_trackPlaylistIds.contains(plId)) {
                    m_trackPlaylistIds.append(plId);
                    emit trackPlaylistsChanged();
                } else if (!found && m_trackPlaylistIds.contains(plId)) {
                    m_trackPlaylistIds.removeAll(plId);
                    emit trackPlaylistsChanged();
                }
            }
        });
    }
}

void Backend::toggleTrackInPlaylist(const QString &playlistId) {
    if (!m_spotifyConnected || m_spotifyAccessToken.isEmpty() || m_currentTrack.spotifyId.isEmpty())
        return;

    const QString trackUri = m_currentTrack.spotifyUri.isEmpty()
                           ? QString("spotify:track:%1").arg(m_currentTrack.spotifyId)
                           : m_currentTrack.spotifyUri;

    const bool isIn = m_trackPlaylistIds.contains(playlistId);
    QUrl url(QString("https://api.spotify.com/v1/playlists/%1/items").arg(playlistId));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", "Bearer " + m_spotifyAccessToken.toUtf8());

    if (isIn) {
        // Remove track
        QJsonObject itemObj;
        itemObj["uri"] = trackUri;
        QJsonArray itemsArr;
        itemsArr.append(itemObj);
        QJsonObject root;
        root["items"] = itemsArr;

        QNetworkReply *reply = m_network.sendCustomRequest(req, "DELETE", QJsonDocument(root).toJson());
        connect(reply, &QNetworkReply::finished, this, [this, reply, playlistId]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                m_trackPlaylistIds.removeAll(playlistId);
                emit trackPlaylistsChanged();
                setStatusText(tr("Removed from playlist."));
                fetchSpotifyPlaylists();
            } else {
                setStatusText(tr("Failed to remove from playlist: %1").arg(reply->errorString()));
            }
        });
    } else {
        // Add track
        QJsonArray urisArr;
        urisArr.append(trackUri);
        QJsonObject root;
        root["uris"] = urisArr;

        QNetworkReply *reply = m_network.post(req, QJsonDocument(root).toJson());
        connect(reply, &QNetworkReply::finished, this, [this, reply, playlistId]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                if (!m_trackPlaylistIds.contains(playlistId))
                    m_trackPlaylistIds.append(playlistId);
                emit trackPlaylistsChanged();
                setStatusText(tr("Added to playlist!"));
                fetchSpotifyPlaylists();
            } else {
                setStatusText(tr("Failed to add to playlist: %1").arg(reply->errorString()));
            }
        });
    }
}

void Backend::toggleLikedSongs() {
    if (!m_spotifyConnected || m_spotifyAccessToken.isEmpty() || m_currentTrack.spotifyId.isEmpty())
        return;

    const QString trackUri = m_currentTrack.spotifyUri.isEmpty()
                           ? QString("spotify:track:%1").arg(m_currentTrack.spotifyId)
                           : m_currentTrack.spotifyUri;

    QUrl url("https://api.spotify.com/v1/me/library");
    QUrlQuery q;
    q.addQueryItem("uris", trackUri);
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_spotifyAccessToken.toUtf8());

    if (m_isTrackLiked) {
        QNetworkReply *reply = m_network.deleteResource(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                m_isTrackLiked = false;
                emit trackPlaylistsChanged();
                setStatusText(tr("Removed from Liked Songs."));
            } else {
                setStatusText(tr("Failed to update Liked Songs: %1").arg(reply->errorString()));
            }
        });
    } else {
        QNetworkReply *reply = m_network.put(req, QByteArray());
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                m_isTrackLiked = true;
                emit trackPlaylistsChanged();
                setStatusText(tr("Added to Liked Songs!"));
            } else {
                setStatusText(tr("Failed to update Liked Songs: %1").arg(reply->errorString()));
            }
        });
    }
}

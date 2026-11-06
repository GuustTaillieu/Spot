#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QProcess>
#include <QTcpServer>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

struct TrackInfo {
    QString title;
    QString artist;
    QString album;
    QString year;
    QString coverUrl;
    QString shazamUrl;
    QString spotifyId;
    QString spotifyUrl;
    QString spotifyUri;
    QDateTime timestamp;

    static TrackInfo fromShazamJson(const QByteArray &json);
    QVariantMap toMap() const;
    static TrackInfo fromMap(const QVariantMap &map);
};

class Backend : public QObject {
    Q_OBJECT

    // Recognition & Audio properties
    Q_PROPERTY(bool isListening READ isListening NOTIFY isListeningChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QVariantList audioDevices READ audioDevices NOTIFY audioDevicesChanged)
    Q_PROPERTY(int selectedDeviceIndex READ selectedDeviceIndex WRITE setSelectedDeviceIndex NOTIFY selectedDeviceIndexChanged)

    // Current Track properties
    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY trackChanged)
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY trackChanged)
    Q_PROPERTY(QString trackArtist READ trackArtist NOTIFY trackChanged)
    Q_PROPERTY(QString trackAlbum READ trackAlbum NOTIFY trackChanged)
    Q_PROPERTY(QString trackYear READ trackYear NOTIFY trackChanged)
    Q_PROPERTY(QString trackCoverUrl READ trackCoverUrl NOTIFY trackChanged)
    Q_PROPERTY(QString trackSpotifyUrl READ trackSpotifyUrl NOTIFY trackChanged)
    Q_PROPERTY(QString trackSpotifyUri READ trackSpotifyUri NOTIFY trackChanged)
    Q_PROPERTY(QString trackSpotifyId READ trackSpotifyId NOTIFY trackChanged)
    Q_PROPERTY(QString trackShazamUrl READ trackShazamUrl NOTIFY trackChanged)

    // Spotify Integration properties
    Q_PROPERTY(bool spotifyConnected READ spotifyConnected NOTIFY spotifyConnectedChanged)
    Q_PROPERTY(QString spotifyUser READ spotifyUser NOTIFY spotifyUserChanged)
    Q_PROPERTY(QString spotifyClientId READ spotifyClientId WRITE setSpotifyClientId NOTIFY spotifyClientIdChanged)
    Q_PROPERTY(bool isTrackInPlaylists READ isTrackInPlaylists NOTIFY trackPlaylistsChanged)
    Q_PROPERTY(bool isTrackLiked READ isTrackLiked NOTIFY trackPlaylistsChanged)
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY playlistsChanged)
    Q_PROPERTY(QStringList trackPlaylistIds READ trackPlaylistIds NOTIFY trackPlaylistsChanged)

    // History
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    bool isListening() const { return m_isListening; }
    QString statusText() const { return m_statusText; }
    QVariantList audioDevices() const { return m_audioDevices; }
    int selectedDeviceIndex() const { return m_selectedDeviceIndex; }
    void setSelectedDeviceIndex(int index);

    bool hasTrack() const { return m_hasTrack; }
    QString trackTitle() const { return m_currentTrack.title; }
    QString trackArtist() const { return m_currentTrack.artist; }
    QString trackAlbum() const { return m_currentTrack.album; }
    QString trackYear() const { return m_currentTrack.year; }
    QString trackCoverUrl() const { return m_currentTrack.coverUrl; }
    QString trackSpotifyUrl() const { return m_currentTrack.spotifyUrl; }
    QString trackSpotifyUri() const { return m_currentTrack.spotifyUri; }
    QString trackSpotifyId() const { return m_currentTrack.spotifyId; }
    QString trackShazamUrl() const { return m_currentTrack.shazamUrl; }

    bool spotifyConnected() const { return m_spotifyConnected; }
    QString spotifyUser() const { return m_spotifyUser; }
    QString spotifyClientId() const { return m_spotifyClientId; }
    void setSpotifyClientId(const QString &id);

    bool isTrackInPlaylists() const { return !m_trackPlaylistIds.isEmpty() || m_isTrackLiked; }
    bool isTrackLiked() const { return m_isTrackLiked; }
    QVariantList playlists() const { return m_playlists; }
    QStringList trackPlaylistIds() const { return m_trackPlaylistIds; }
    QVariantList history() const { return m_history; }

    // QML Actions
    Q_INVOKABLE void primaryAction(); // Triggered by Space
    Q_INVOKABLE void startListening();
    Q_INVOKABLE void cancelListening();
    Q_INVOKABLE void refreshAudioDevices();

    Q_INVOKABLE void openSpotify();
    Q_INVOKABLE void copySpotifyLink();
    Q_INVOKABLE void openShazam();

    Q_INVOKABLE void startSpotifyAuth();
    Q_INVOKABLE void disconnectSpotify();
    Q_INVOKABLE void refreshPlaylists();
    Q_INVOKABLE void toggleTrackInPlaylist(const QString &playlistId);
    Q_INVOKABLE void toggleLikedSongs();

    Q_INVOKABLE void selectHistoryTrack(int index);
    Q_INVOKABLE void clearHistory();

signals:
    void isListeningChanged();
    void statusTextChanged();
    void audioDevicesChanged();
    void selectedDeviceIndexChanged();
    void trackChanged();
    void spotifyConnectedChanged();
    void spotifyUserChanged();
    void spotifyClientIdChanged();
    void trackPlaylistsChanged();
    void playlistsChanged();
    void historyChanged();

private slots:
    void onFfmpegFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onSongrecFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onAuthServerNewConnection();

private:
    void setStatusText(const QString &text);
    void loadSettings();
    void saveSettings();
    void loadHistory();
    void saveHistory();
    void addToHistory(const TrackInfo &track);

    void searchSpotifyTrack(const QString &title, const QString &artist);
    void checkTrackPlaylists();
    void fetchSpotifyPlaylists();
    void fetchSpotifyUserProfile();
    void refreshSpotifyToken();

    bool m_isListening = false;
    QString m_statusText;
    QVariantList m_audioDevices;
    int m_selectedDeviceIndex = 0;

    bool m_hasTrack = false;
    TrackInfo m_currentTrack;

    bool m_spotifyConnected = false;
    QString m_spotifyUser;
    QString m_spotifyClientId = "1874ebba0eb8472f8832a514d45ad6aa"; // Default Spotify desktop client ID
    QString m_spotifyAccessToken;
    QString m_spotifyRefreshToken;
    QDateTime m_spotifyTokenExpiresAt;
    QString m_pkceVerifier;

    bool m_isTrackLiked = false;
    QStringList m_trackPlaylistIds;
    QVariantList m_playlists;
    QVariantList m_history;

    QProcess m_recordingProcess;
    QProcess m_songrecProcess;
    QTcpServer m_authServer;
    QNetworkAccessManager m_network;
    QString m_tempWavPath = "/tmp/spot_sample.wav";
};

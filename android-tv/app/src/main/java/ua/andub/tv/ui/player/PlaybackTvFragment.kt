package ua.andub.tv.ui.player

import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.media3.common.MediaItem
import androidx.media3.common.MimeTypes
import androidx.media3.common.PlaybackException
import androidx.media3.common.Player
import androidx.media3.datasource.DefaultHttpDataSource
import androidx.media3.exoplayer.ExoPlayer
import androidx.media3.exoplayer.hls.HlsMediaSource
import androidx.media3.exoplayer.source.DefaultMediaSourceFactory
import androidx.media3.exoplayer.source.MediaSource
import androidx.media3.ui.PlayerView
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import ua.andub.tv.R
import ua.andub.tv.data.models.Anime
import ua.andub.tv.data.models.Episode
import ua.andub.tv.data.models.Quality
import ua.andub.tv.data.models.Stream
import ua.andub.tv.data.providers.NetworkClient
import ua.andub.tv.data.providers.ProviderRegistry
import ua.andub.tv.data.storage.HistoryManager

class PlaybackTvFragment : Fragment() {

    private lateinit var anime: Anime
    private lateinit var episode: Episode
    private var allEpisodes = listOf<Episode>()

    private var player: ExoPlayer? = null
    private lateinit var playerView: PlayerView
    private lateinit var loadingSpinner: ProgressBar
    private lateinit var errorText: TextView

    private var currentStream: Stream? = null
    private var currentQualityIndex = 0
    private var progressTrackingJob: Job? = null

    companion object {
        const val EXTRA_ANIME = "extra_anime"
        const val EXTRA_EPISODE = "extra_episode"
        const val EXTRA_ALL_EPISODES = "extra_all_episodes"

        fun newInstance(anime: Anime, episode: Episode, episodes: List<Episode>): PlaybackTvFragment {
            return PlaybackTvFragment().apply {
                arguments = Bundle().apply {
                    putSerializable(EXTRA_ANIME, anime)
                    putSerializable(EXTRA_EPISODE, episode)
                    putSerializable(EXTRA_ALL_EPISODES, ArrayList(episodes))
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        anime = arguments?.getSerializable(EXTRA_ANIME) as? Anime ?: return
        episode = arguments?.getSerializable(EXTRA_EPISODE) as? Episode ?: return
        @Suppress("UNCHECKED_CAST")
        allEpisodes = (arguments?.getSerializable(EXTRA_ALL_EPISODES) as? ArrayList<Episode>) ?: emptyList()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_playback, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        playerView = view.findViewById(R.id.player_view)
        loadingSpinner = view.findViewById(R.id.loading_spinner)
        errorText = view.findViewById(R.id.error_text)

        loadAndPlay()
    }

    private fun loadAndPlay() {
        loadingSpinner.visibility = View.VISIBLE
        errorText.visibility = View.GONE

        lifecycleScope.launch {
            try {
                val provider = ProviderRegistry.getProvider(anime.provider)
                val stream = provider.getStream(anime, episode)
                currentStream = stream
                currentQualityIndex = 0

                val candidates = getPlayableCandidates(stream)
                if (candidates.isEmpty()) {
                    loadingSpinner.visibility = View.GONE
                    errorText.text = "Не вдалося отримати потік відео для цього епізоду"
                    errorText.visibility = View.VISIBLE
                    return@launch
                }

                playQualityAt(0)
            } catch (e: Exception) {
                e.printStackTrace()
                loadingSpinner.visibility = View.GONE
                errorText.text = "Помилка завантаження: ${e.message}"
                errorText.visibility = View.VISIBLE
            }
        }
    }

    private fun getPlayableCandidates(stream: Stream): List<Quality> {
        val list = mutableListOf<Quality>()
        for (q in stream.qualities) {
            if (q.url.isNotBlank()) list.add(q)
        }
        if (list.isEmpty() && stream.url.isNotBlank()) {
            list.add(Quality(stream.quality, stream.url, stream.headers))
        }
        return list
    }

    private fun playQualityAt(index: Int) {
        val stream = currentStream ?: return
        val candidates = getPlayableCandidates(stream)
        if (index >= candidates.size) {
            loadingSpinner.visibility = View.GONE
            errorText.text = "Помилка відтворення: жодна якість не доступна"
            errorText.visibility = View.VISIBLE
            return
        }

        currentQualityIndex = index
        val target = candidates[index]
        var targetUrl = target.url
        if (targetUrl.startsWith("//")) {
            targetUrl = "https:$targetUrl"
        }

        val targetHeaders = target.headers.ifEmpty { stream.primaryHeaders }
        initPlayer(targetUrl, targetHeaders)
    }

    private fun initPlayer(streamUrl: String, headers: Map<String, String>) {
        releasePlayer()

        val httpHeaders = mutableMapOf<String, String>()
        httpHeaders["User-Agent"] = headers["User-Agent"] ?: NetworkClient.USER_AGENT
        headers.forEach { (k, v) ->
            if (v.isNotBlank()) httpHeaders[k] = v
        }

        val okHttpDataSourceFactory = androidx.media3.datasource.okhttp.OkHttpDataSource.Factory(NetworkClient.client).apply {
            setUserAgent(httpHeaders["User-Agent"] ?: NetworkClient.USER_AGENT)
            setDefaultRequestProperties(httpHeaders)
        }

        val isHls = streamUrl.contains(".m3u8", ignoreCase = true) || streamUrl.contains("hls", ignoreCase = true)
        val isMp4 = streamUrl.contains(".mp4", ignoreCase = true)

        // DefaultMediaSourceFactory in Media3 seamlessly handles HLS, MP4, and DASH
        // without crashing on missing CODECS attributes in master playlists
        val mediaSourceFactory = DefaultMediaSourceFactory(okHttpDataSourceFactory)

        val exoPlayer = ExoPlayer.Builder(requireContext())
            .setMediaSourceFactory(mediaSourceFactory)
            .build()

        player = exoPlayer
        playerView.player = exoPlayer

        // Update titles in custom controller layout
        playerView.findViewById<TextView>(R.id.player_anime_title)?.text = anime.primaryTitle
        val cleanEpTitle = episode.title.replace("— null", "").replace("- null", "").trim()
        playerView.findViewById<TextView>(R.id.player_episode_title)?.text = if (cleanEpTitle.isNotBlank() && cleanEpTitle != "null") cleanEpTitle else "Серія ${episode.number}"

        val mediaItemBuilder = MediaItem.Builder().setUri(Uri.parse(streamUrl))
        if (isHls) {
            mediaItemBuilder.setMimeType(MimeTypes.APPLICATION_M3U8)
        } else if (isMp4) {
            mediaItemBuilder.setMimeType(MimeTypes.APPLICATION_MP4)
        }
        val mediaItem = mediaItemBuilder.build()
        exoPlayer.setMediaItem(mediaItem)

        // Resume position from HistoryManager
        val savedProgress = HistoryManager.getProgress(requireContext(), anime.id, episode.number)
        if (savedProgress > 0) {
            exoPlayer.seekTo(savedProgress)
        }

        exoPlayer.addListener(object : Player.Listener {
            override fun onPlaybackStateChanged(state: Int) {
                when (state) {
                    Player.STATE_BUFFERING -> loadingSpinner.visibility = View.VISIBLE
                    Player.STATE_READY -> {
                        loadingSpinner.visibility = View.GONE
                        errorText.visibility = View.GONE
                    }
                    Player.STATE_ENDED -> onPlaybackEnded()
                    Player.STATE_IDLE -> {}
                }
            }

            override fun onPlayerError(error: PlaybackException) {
                loadingSpinner.visibility = View.GONE
                val stream = currentStream
                val candidates = stream?.let { getPlayableCandidates(it) } ?: emptyList()

                // Try next quality if available
                if (currentQualityIndex + 1 < candidates.size) {
                    val nextIndex = currentQualityIndex + 1
                    Toast.makeText(requireContext(), "Спроба якості ${candidates[nextIndex].label}...", Toast.LENGTH_SHORT).show()
                    playQualityAt(nextIndex)
                } else {
                    val detail = error.cause?.message ?: error.message ?: error.errorCodeName
                    errorText.text = "Помилка джерела відео: $detail\nПровайдер: ${anime.provider}. Спробуйте іншого провайдера."
                    errorText.visibility = View.VISIBLE
                }
            }
        })

        exoPlayer.prepare()
        exoPlayer.playWhenReady = true

        startProgressTracking()
    }

    private fun startProgressTracking() {
        progressTrackingJob?.cancel()
        progressTrackingJob = lifecycleScope.launch {
            while (isActive) {
                delay(5000)
                saveCurrentProgress()
            }
        }
    }

    private fun saveCurrentProgress() {
        val p = player ?: return
        val pos = p.currentPosition
        val dur = p.duration
        if (pos > 0 && dur > 0) {
            HistoryManager.saveProgress(
                requireContext(),
                anime,
                episode.number,
                pos,
                dur
            )
        }
    }

    private fun onPlaybackEnded() {
        val currentIndex = allEpisodes.indexOfFirst { it.number == episode.number }
        if (currentIndex != -1 && currentIndex + 1 < allEpisodes.size) {
            val nextEp = allEpisodes[currentIndex + 1]
            Toast.makeText(requireContext(), "Перехід до наступної серії: ${nextEp.title}", Toast.LENGTH_SHORT).show()
            episode = nextEp
            loadAndPlay()
        } else {
            Toast.makeText(requireContext(), "Перегляд завершено", Toast.LENGTH_SHORT).show()
        }
    }

    private fun releasePlayer() {
        progressTrackingJob?.cancel()
        saveCurrentProgress()
        player?.release()
        player = null
    }

    override fun onPause() {
        super.onPause()
        saveCurrentProgress()
        player?.pause()
    }

    override fun onDestroy() {
        super.onDestroy()
        releasePlayer()
    }
}

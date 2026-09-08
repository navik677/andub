package ua.andub.tv.ui.details

import android.content.Context
import android.graphics.Bitmap
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.Drawable
import android.os.Bundle
import android.view.View
import androidx.core.content.ContextCompat
import androidx.leanback.app.BackgroundManager
import androidx.leanback.app.DetailsSupportFragment
import androidx.leanback.widget.Action
import androidx.leanback.widget.ArrayObjectAdapter
import androidx.leanback.widget.ClassPresenterSelector
import androidx.leanback.widget.DetailsOverviewRow
import androidx.leanback.widget.FullWidthDetailsOverviewRowPresenter
import androidx.leanback.widget.HeaderItem
import androidx.leanback.widget.ListRow
import androidx.leanback.widget.ListRowPresenter
import androidx.leanback.widget.OnActionClickedListener
import androidx.lifecycle.lifecycleScope
import com.bumptech.glide.Glide
import com.bumptech.glide.request.target.CustomTarget
import com.bumptech.glide.request.transition.Transition
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import ua.andub.tv.R
import ua.andub.tv.data.models.Anime
import ua.andub.tv.data.models.Episode
import ua.andub.tv.data.providers.ProviderRegistry
import ua.andub.tv.data.storage.FavoritesManager
import ua.andub.tv.ui.common.BlurUtils
import ua.andub.tv.ui.player.PlaybackTvActivity

class DetailsTvFragment : DetailsSupportFragment() {

    private lateinit var anime: Anime
    private lateinit var backgroundManager: BackgroundManager
    private var defaultBackground: Drawable? = null

    private lateinit var rowsAdapter: ArrayObjectAdapter
    private lateinit var detailsOverviewRow: DetailsOverviewRow
    private lateinit var actionsAdapter: ArrayObjectAdapter
    private val episodesAdapter = ArrayObjectAdapter(EpisodeCardPresenter())
    private val episodesList = mutableListOf<Episode>()

    companion object {
        private const val ACTION_PLAY = 1L
        private const val ACTION_FAVORITE = 2L
        const val EXTRA_ANIME = "extra_anime"

        fun newInstance(anime: Anime): DetailsTvFragment {
            return DetailsTvFragment().apply {
                arguments = Bundle().apply {
                    putSerializable(EXTRA_ANIME, anime)
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        anime = arguments?.getSerializable(EXTRA_ANIME) as? Anime ?: return
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        initBackgroundManager()
        setupPresenter()
        buildDetails()
        loadEpisodes()
    }

    private fun initBackgroundManager() {
        backgroundManager = BackgroundManager.getInstance(requireActivity())
        backgroundManager.attach(requireActivity().window)
        defaultBackground = ColorDrawable(ContextCompat.getColor(requireContext(), R.color.brand_background))
        backgroundManager.drawable = defaultBackground

        if (anime.posterUrl.isNotEmpty()) {
            Glide.with(requireContext())
                .asBitmap()
                .load(ua.andub.tv.ui.common.GlideUtils.buildGlideUrl(anime.posterUrl))
                .into(object : CustomTarget<Bitmap>(320, 180) {
                    override fun onResourceReady(resource: Bitmap, transition: Transition<in Bitmap>?) {
                        lifecycleScope.launch(Dispatchers.Default) {
                            val blurred = BlurUtils.blur(resource, radius = 18, darkTintAlpha = 150)
                            withContext(Dispatchers.Main) {
                                backgroundManager.setBitmap(blurred)
                            }
                        }
                    }

                    override fun onLoadCleared(placeholder: Drawable?) {
                        backgroundManager.drawable = defaultBackground
                    }
                })
        }
    }

    private var loadEpisodesJob: kotlinx.coroutines.Job? = null

    private fun setupPresenter() {
        val detailsPresenter = FullWidthDetailsOverviewRowPresenter(DetailsDescriptionPresenter()).apply {
            backgroundColor = ContextCompat.getColor(requireContext(), R.color.card_background)
            initialState = FullWidthDetailsOverviewRowPresenter.STATE_FULL
            onActionClickedListener = OnActionClickedListener { action ->
                when (action?.id) {
                    ACTION_PLAY -> {
                        lifecycleScope.launch {
                            loadEpisodesJob?.join()
                            val ep = episodesList.firstOrNull() ?: Episode("1", "Серія 1", meta = anime.meta)
                            PlaybackTvActivity.start(requireContext(), anime, ep, episodesList)
                        }
                    }
                    ACTION_FAVORITE -> {
                        toggleFavorite()
                    }
                }
            }
        }

        val presenterSelector = ClassPresenterSelector().apply {
            addClassPresenter(DetailsOverviewRow::class.java, detailsPresenter)
            addClassPresenter(ListRow::class.java, ListRowPresenter())
        }

        rowsAdapter = ArrayObjectAdapter(presenterSelector)
        adapter = rowsAdapter

        setOnItemViewClickedListener { _, item, _, _ ->
            if (item is Episode) {
                PlaybackTvActivity.start(requireContext(), anime, item, episodesList)
            }
        }
    }

    private fun buildDetails() {
        detailsOverviewRow = DetailsOverviewRow(anime)

        // Poster image
        if (anime.posterUrl.isNotEmpty()) {
            val width = resources.getDimensionPixelSize(R.dimen.card_width)
            val height = resources.getDimensionPixelSize(R.dimen.card_height)
            Glide.with(requireContext())
                .asBitmap()
                .load(ua.andub.tv.ui.common.GlideUtils.buildGlideUrl(anime.posterUrl))
                .into(object : CustomTarget<Bitmap>(width, height) {
                    override fun onResourceReady(resource: Bitmap, transition: Transition<in Bitmap>?) {
                        detailsOverviewRow.setImageBitmap(requireContext(), resource)
                    }

                    override fun onLoadCleared(placeholder: Drawable?) {}
                })
        }

        // Actions
        val isFav = FavoritesManager.isFavorite(requireContext(), anime)
        actionsAdapter = ArrayObjectAdapter().apply {
            add(Action(ACTION_PLAY, getString(R.string.action_play)))
            add(Action(ACTION_FAVORITE, if (isFav) "У видалених" else getString(R.string.action_favorites)))
        }
        detailsOverviewRow.actionsAdapter = actionsAdapter

        rowsAdapter.add(detailsOverviewRow)

        // Episodes Row
        val epHeader = HeaderItem(1, getString(R.string.header_episodes))
        rowsAdapter.add(ListRow(epHeader, episodesAdapter))
    }

    private fun toggleFavorite() {
        val isFav = FavoritesManager.isFavorite(requireContext(), anime)
        if (isFav) {
            FavoritesManager.removeFavorite(requireContext(), anime)
        } else {
            FavoritesManager.addFavorite(requireContext(), anime)
        }
        val newFav = !isFav
        actionsAdapter.replace(
            1,
            Action(ACTION_FAVORITE, if (newFav) "У видалених" else getString(R.string.action_favorites))
        )
    }

    private fun loadEpisodes() {
        loadEpisodesJob?.cancel()
        loadEpisodesJob = lifecycleScope.launch {
            try {
                val provider = ProviderRegistry.getProvider(anime.provider)
                val eps = withContext(Dispatchers.IO) { provider.getEpisodes(anime) }
                if (eps.isNotEmpty()) {
                    episodesList.clear()
                    episodesList.addAll(eps)
                    episodesAdapter.clear()
                    eps.forEach { episodesAdapter.add(it) }
                } else if (episodesList.isEmpty()) {
                    val initialFallback = Episode("1", "Серія 1", meta = anime.meta)
                    episodesList.add(initialFallback)
                    episodesAdapter.add(initialFallback)
                }
            } catch (e: Exception) {
                e.printStackTrace()
                if (episodesList.isEmpty()) {
                    val initialFallback = Episode("1", "Серія 1", meta = anime.meta)
                    episodesList.add(initialFallback)
                    episodesAdapter.add(initialFallback)
                }
            }
        }
    }
}

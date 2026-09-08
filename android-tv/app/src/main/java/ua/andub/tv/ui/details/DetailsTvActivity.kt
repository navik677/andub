package ua.andub.tv.ui.details

import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.ImageView
import android.widget.ProgressBar
import android.widget.TextView
import androidx.fragment.app.FragmentActivity
import androidx.gridlayout.widget.GridLayout
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.bumptech.glide.Glide
import com.bumptech.glide.request.target.CustomTarget
import com.bumptech.glide.request.transition.Transition
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import ua.andub.tv.R
import ua.andub.tv.data.models.Anime
import ua.andub.tv.data.models.Episode
import ua.andub.tv.data.providers.ProviderRegistry
import ua.andub.tv.data.storage.FavoritesManager
import ua.andub.tv.data.storage.HistoryManager
import ua.andub.tv.ui.common.BlurUtils
import ua.andub.tv.ui.common.GlideUtils
import ua.andub.tv.ui.player.PlaybackTvActivity

class DetailsTvActivity : FragmentActivity() {

    private lateinit var anime: Anime
    private var episodesList = mutableListOf<Episode>()
    private lateinit var episodeAdapter: EpisodePcAdapter
    private var loadEpisodesJob: Job? = null

    private lateinit var ivAmbientBg: ImageView
    private lateinit var ivPoster: ImageView
    private lateinit var btnBack: Button
    private lateinit var btnFavorite: Button
    private lateinit var gridGenres: GridLayout
    private lateinit var tvTitle: TextView
    private lateinit var tvOrigTitle: TextView
    private lateinit var badgeYear: TextView
    private lateinit var badgeRating: TextView
    private lateinit var badgeProvider: TextView
    private lateinit var badgeType: TextView
    private lateinit var tvDescription: TextView
    private lateinit var tvEpisodesHeader: TextView
    private lateinit var pbEpisodesLoading: ProgressBar
    private lateinit var rvEpisodes: RecyclerView

    companion object {
        const val EXTRA_ANIME = "extra_anime"

        fun start(context: Context, anime: Anime) {
            val intent = Intent(context, DetailsTvActivity::class.java).apply {
                putExtra(EXTRA_ANIME, anime)
            }
            context.startActivity(intent)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_details_tv)

        @Suppress("DEPRECATION")
        anime = intent.getSerializableExtra(EXTRA_ANIME) as? Anime ?: run {
            finish()
            return
        }

        initViews()
        bindAnimeDetails()
        setupEpisodesList()
        loadEpisodes()
    }

    override fun onResume() {
        super.onResume()
        updateWatchedCount()
        episodeAdapter.notifyDataSetChanged()
    }

    private fun initViews() {
        ivAmbientBg = findViewById(R.id.iv_ambient_bg)
        ivPoster = findViewById(R.id.iv_poster)
        btnBack = findViewById(R.id.btn_back)
        btnFavorite = findViewById(R.id.btn_favorite)
        gridGenres = findViewById(R.id.grid_genres)
        tvTitle = findViewById(R.id.tv_title)
        tvOrigTitle = findViewById(R.id.tv_orig_title)
        badgeYear = findViewById(R.id.badge_year)
        badgeRating = findViewById(R.id.badge_rating)
        badgeProvider = findViewById(R.id.badge_provider)
        badgeType = findViewById(R.id.badge_type)
        tvDescription = findViewById(R.id.tv_description)
        tvEpisodesHeader = findViewById(R.id.tv_episodes_header)
        pbEpisodesLoading = findViewById(R.id.pb_episodes_loading)
        rvEpisodes = findViewById(R.id.rv_episodes)

        btnBack.setOnClickListener { finish() }
        btnFavorite.setOnClickListener { toggleFavorite() }
    }

    private fun bindAnimeDetails() {
        // 1. Ambient blurred backdrop
        if (anime.posterUrl.isNotEmpty()) {
            Glide.with(this)
                .asBitmap()
                .load(GlideUtils.buildGlideUrl(anime.posterUrl))
                .into(object : CustomTarget<Bitmap>(320, 180) {
                    override fun onResourceReady(resource: Bitmap, transition: Transition<in Bitmap>?) {
                        lifecycleScope.launch(Dispatchers.Default) {
                            val blurred = BlurUtils.blur(resource, radius = 22, darkTintAlpha = 50)
                            withContext(Dispatchers.Main) {
                                ivAmbientBg.setImageBitmap(blurred)
                            }
                        }
                    }
                    override fun onLoadCleared(placeholder: android.graphics.drawable.Drawable?) {}
                })
        }

        // 2. Poster
        if (anime.posterUrl.isNotEmpty()) {
            Glide.with(this)
                .load(GlideUtils.buildGlideUrl(anime.posterUrl))
                .centerCrop()
                .into(ivPoster)
        }

        // 3. Titles & Badges
        tvTitle.text = anime.primaryTitle
        if (anime.titleEn.isNotBlank() && anime.titleEn != anime.titleRu) {
            tvOrigTitle.text = anime.titleEn
            tvOrigTitle.visibility = View.VISIBLE
        } else {
            tvOrigTitle.visibility = View.GONE
        }

        badgeYear.text = if (anime.year > 0) anime.year.toString() else "2024"
        badgeRating.text = anime.rating.ifEmpty { "★ 8.5" }
        badgeProvider.text = anime.provider
        badgeType.text = "TV"

        // 4. Description
        val cleanDesc = anime.description
            .replace(Regex("<[^>]*>"), "")
            .replace(Regex("&quot;"), "\"")
            .replace(Regex("&amp;"), "&")
            .trim()
        tvDescription.text = if (cleanDesc.isNotBlank() && cleanDesc != "null" && cleanDesc != "None") cleanDesc else "Опис аніме тимчасово відсутній"

        // 5. Favorite Button State
        updateFavoriteButton()

        // 6. Genres Grid
        populateGenres()
    }

    private fun populateGenres() {
        gridGenres.removeAllViews()
        val genres = anime.genres.filter { it.isNotBlank() }.take(6)
        if (genres.isEmpty()) {
            val defaultGenres = listOf("Аніме", anime.provider)
            defaultGenres.forEach { addGenrePill(it) }
        } else {
            genres.forEach { addGenrePill(it) }
        }
    }

    private fun addGenrePill(genre: String) {
        val tv = TextView(this).apply {
            text = genre
            setTextColor(0xFFCBD5E1.toInt())
            textSize = 11f
            gravity = Gravity.CENTER
            setBackgroundResource(R.drawable.bg_button_pc_pill)
            setPadding(8, 4, 8, 4)
            isClickable = true
            isFocusable = true
            setOnClickListener {
                ua.andub.tv.ui.search.SearchTvActivity.startWithGenre(this@DetailsTvActivity, genre)
            }
        }
        val params = GridLayout.LayoutParams().apply {
            width = (100 * resources.displayMetrics.density).toInt()
            height = (28 * resources.displayMetrics.density).toInt()
            setMargins(4, 4, 4, 4)
        }
        gridGenres.addView(tv, params)
    }

    private fun updateFavoriteButton() {
        val isFav = FavoritesManager.isFavorite(this, anime)
        btnFavorite.text = if (isFav) "★ В улюблених" else "+ Додати в улюблені"
    }

    private fun toggleFavorite() {
        val isFav = FavoritesManager.isFavorite(this, anime)
        if (isFav) {
            FavoritesManager.removeFavorite(this, anime)
        } else {
            FavoritesManager.addFavorite(this, anime)
        }
        updateFavoriteButton()
    }

    private fun setupEpisodesList() {
        episodeAdapter = EpisodePcAdapter(
            episodes = episodesList,
            isWatched = { epNum ->
                HistoryManager.getProgress(this, anime.id, epNum) > 0
            },
            onItemClick = { episode ->
                PlaybackTvActivity.start(this, anime, episode, episodesList)
            }
        )

        rvEpisodes.layoutManager = LinearLayoutManager(this)
        rvEpisodes.adapter = episodeAdapter
    }

    private fun updateWatchedCount() {
        var watchedCount = 0
        for (ep in episodesList) {
            if (HistoryManager.getProgress(this, anime.id, ep.number) > 0) {
                watchedCount++
            }
        }
        tvEpisodesHeader.text = "Список серій:  Переглянуто: $watchedCount / ${episodesList.size}"
    }

    private fun loadEpisodes() {
        // Initial fallback
        val initialFallback = Episode("1", "Серія 1", meta = anime.meta)
        episodesList.clear()
        episodesList.add(initialFallback)
        episodeAdapter.updateEpisodes(episodesList)
        updateWatchedCount()

        pbEpisodesLoading.visibility = View.VISIBLE

        loadEpisodesJob?.cancel()
        loadEpisodesJob = lifecycleScope.launch {
            try {
                val provider = ProviderRegistry.getProvider(anime.provider)
                val eps = withContext(Dispatchers.IO) { provider.getEpisodes(anime) }
                if (eps.isNotEmpty()) {
                    episodesList.clear()
                    episodesList.addAll(eps)
                    episodeAdapter.updateEpisodes(episodesList)
                    updateWatchedCount()
                }
            } catch (e: Exception) {
                e.printStackTrace()
            } finally {
                pbEpisodesLoading.visibility = View.GONE
            }
        }
    }
}

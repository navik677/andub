package ua.andub.tv.ui.search

import android.graphics.Bitmap
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.Drawable
import android.os.Bundle
import android.view.View
import androidx.core.content.ContextCompat
import androidx.leanback.app.BackgroundManager
import androidx.leanback.app.SearchSupportFragment
import androidx.leanback.widget.ArrayObjectAdapter
import androidx.leanback.widget.HeaderItem
import androidx.leanback.widget.ListRow
import androidx.leanback.widget.ListRowPresenter
import androidx.leanback.widget.ObjectAdapter
import androidx.lifecycle.lifecycleScope
import com.bumptech.glide.Glide
import com.bumptech.glide.request.target.CustomTarget
import com.bumptech.glide.request.transition.Transition
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import ua.andub.tv.R
import ua.andub.tv.data.models.Anime
import ua.andub.tv.data.providers.ProviderRegistry
import ua.andub.tv.ui.browse.presenters.AnimeCardPresenter
import ua.andub.tv.ui.browse.presenters.GenreCardPresenter
import ua.andub.tv.ui.common.BlurUtils
import ua.andub.tv.ui.common.GlideUtils
import ua.andub.tv.ui.details.DetailsTvActivity

class SearchTvFragment : SearchSupportFragment(), SearchSupportFragment.SearchResultProvider {

    companion object {
        private const val ARG_GENRE = "arg_genre"
        private const val ARG_QUERY = "arg_query"

        val ALL_GENRES = listOf(
            "Екшн", "Комедія", "Фентезі", "Романтика", "Пригоди", "Драма",
            "Сьонен", "Детектив", "Жахи", "Містика", "Спорт", "Фантастика",
            "Повсякденність", "Трилер", "Школа", "Ісекай", "Меха", "Магія", "Хентай"
        )

        fun newInstance(genre: String? = null, initialQuery: String? = null): SearchTvFragment {
            val fragment = SearchTvFragment()
            val args = Bundle().apply {
                putString(ARG_GENRE, genre)
                putString(ARG_QUERY, initialQuery)
            }
            fragment.arguments = args
            return fragment
        }
    }

    private lateinit var backgroundManager: BackgroundManager
    private var defaultBackground: Drawable? = null

    private val rowsAdapter = ArrayObjectAdapter(ListRowPresenter())
    private val searchResultsAdapter = ArrayObjectAdapter(AnimeCardPresenter())
    private val genresAdapter by lazy {
        ArrayObjectAdapter(GenreCardPresenter { selectedGenre ->
            searchByGenre(selectedGenre)
        })
    }

    private var resultsHeader = HeaderItem(0, "Результати пошуку")
    private var searchJob: Job? = null
    private var currentGenre: String? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setSearchResultProvider(this)

        currentGenre = arguments?.getString(ARG_GENRE)
        val initialQuery = arguments?.getString(ARG_QUERY)

        // 1. Search Results Row
        val initialTitle = if (!currentGenre.isNullOrEmpty()) {
            "Жанр: $currentGenre (${ProviderRegistry.currentProvider.displayName})"
        } else {
            "Результати пошуку (${ProviderRegistry.currentProvider.displayName})"
        }
        resultsHeader = HeaderItem(0, initialTitle)
        rowsAdapter.add(ListRow(resultsHeader, searchResultsAdapter))

        // 2. Quick Genre Selector Row
        ALL_GENRES.forEach { genresAdapter.add(it) }
        val genresHeader = HeaderItem(1, "Швидкий пошук за жанрами")
        rowsAdapter.add(ListRow(genresHeader, genresAdapter))

        setOnItemViewClickedListener { _, item, _, _ ->
            when (item) {
                is Anime -> {
                    DetailsTvActivity.start(requireContext(), item)
                }
                is String -> {
                    searchByGenre(item)
                }
            }
        }

        setOnItemViewSelectedListener { _, item, _, _ ->
            if (item is Anime) {
                updateAmbientBlurBackground(item.posterUrl)
            }
        }

        // Auto-run search if opened with genre or query
        if (!currentGenre.isNullOrEmpty()) {
            searchByGenre(currentGenre!!)
        } else if (!initialQuery.isNullOrEmpty()) {
            setSearchQuery(initialQuery, true)
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        initBackgroundManager()
    }

    private fun initBackgroundManager() {
        backgroundManager = BackgroundManager.getInstance(requireActivity())
        backgroundManager.attach(requireActivity().window)
        defaultBackground = ColorDrawable(ContextCompat.getColor(requireContext(), R.color.brand_background))
        backgroundManager.drawable = defaultBackground
    }

    private fun updateAmbientBlurBackground(posterUrl: String) {
        if (posterUrl.isEmpty()) {
            backgroundManager.drawable = defaultBackground
            return
        }

        Glide.with(requireContext())
            .asBitmap()
            .load(GlideUtils.buildGlideUrl(posterUrl))
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

    override fun getResultsAdapter(): ObjectAdapter = rowsAdapter

    override fun onQueryTextChange(newQuery: String): Boolean {
        performSearch(newQuery)
        return true
    }

    override fun onQueryTextSubmit(query: String): Boolean {
        performSearch(query)
        return true
    }

    fun searchByGenre(genre: String) {
        currentGenre = genre
        searchJob?.cancel()
        searchResultsAdapter.clear()

        val newTitle = "Жанр: $genre (${ProviderRegistry.currentProvider.displayName})"
        rowsAdapter.replace(0, ListRow(HeaderItem(0, newTitle), searchResultsAdapter))

        searchJob = lifecycleScope.launch {
            try {
                val results = withContext(Dispatchers.IO) {
                    ProviderRegistry.currentProvider.search(query = "", genre = genre, limit = 40)
                }
                searchResultsAdapter.clear()
                results.forEach { searchResultsAdapter.add(it) }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    private fun performSearch(query: String) {
        searchJob?.cancel()
        if (query.trim().isEmpty()) {
            searchResultsAdapter.clear()
            return
        }

        currentGenre = null
        val newTitle = "Пошук: \"${query.trim()}\" (${ProviderRegistry.currentProvider.displayName})"
        rowsAdapter.replace(0, ListRow(HeaderItem(0, newTitle), searchResultsAdapter))

        searchJob = lifecycleScope.launch {
            delay(400) // Debounce typing on TV
            try {
                val results = withContext(Dispatchers.IO) {
                    ProviderRegistry.currentProvider.search(query.trim(), limit = 40)
                }
                searchResultsAdapter.clear()
                results.forEach { searchResultsAdapter.add(it) }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }
}

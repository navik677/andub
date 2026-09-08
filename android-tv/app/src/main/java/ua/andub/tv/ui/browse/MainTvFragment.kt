package ua.andub.tv.ui.browse

import android.graphics.Bitmap
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.Drawable
import android.os.Bundle
import android.view.View
import androidx.core.content.ContextCompat
import androidx.leanback.app.BackgroundManager
import androidx.leanback.app.BrowseSupportFragment
import androidx.leanback.widget.ArrayObjectAdapter
import androidx.leanback.widget.ListRow
import androidx.leanback.widget.Presenter
import androidx.leanback.widget.PresenterSelector
import androidx.lifecycle.lifecycleScope
import com.bumptech.glide.Glide
import com.bumptech.glide.request.target.CustomTarget
import com.bumptech.glide.request.transition.Transition
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import ua.andub.tv.R
import ua.andub.tv.data.models.Anime
import ua.andub.tv.data.providers.BaseProvider
import ua.andub.tv.data.providers.ProviderRegistry
import ua.andub.tv.data.storage.FavoritesManager
import ua.andub.tv.data.storage.HistoryManager
import ua.andub.tv.ui.browse.presenters.AnimeCardPresenter
import ua.andub.tv.ui.browse.presenters.GridListRowPresenter
import ua.andub.tv.ui.browse.presenters.IconRowHeaderPresenter
import ua.andub.tv.ui.browse.presenters.ProviderCardPresenter
import ua.andub.tv.ui.browse.rows.IconHeaderItem
import ua.andub.tv.ui.common.BlurUtils
import ua.andub.tv.ui.details.DetailsTvActivity
import ua.andub.tv.ui.search.SearchTvActivity
import java.util.Collections

class MainTvFragment : BrowseSupportFragment() {

    private lateinit var backgroundManager: BackgroundManager
    private var defaultBackground: Drawable? = null

    private lateinit var rowsAdapter: ArrayObjectAdapter
    private val cardPresenter = AnimeCardPresenter()

    // Dynamic row adapters
    private val popularAdapter = ArrayObjectAdapter(cardPresenter)
    private val latestAdapter = ArrayObjectAdapter(cardPresenter)
    private val favoritesAdapter = ArrayObjectAdapter(cardPresenter)
    private val historyAdapter = ArrayObjectAdapter(cardPresenter)
    private val genresAdapter by lazy {
        ArrayObjectAdapter(ua.andub.tv.ui.browse.presenters.GenreCardPresenter { genre ->
            SearchTvActivity.startWithGenre(requireContext(), genre)
        })
    }
    private val providersAdapter by lazy {
        ArrayObjectAdapter(ProviderCardPresenter { provider ->
            switchProvider(provider)
        })
    }

    private var loadDataJob: Job? = null

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        initBackgroundManager()
        setupUIElements()
        setupRows()
        setupEventListeners()

        loadAllData()
    }

    override fun onResume() {
        super.onResume()
        refreshLocalRows()
    }

    private fun initBackgroundManager() {
        backgroundManager = BackgroundManager.getInstance(requireActivity())
        backgroundManager.attach(requireActivity().window)
        defaultBackground = ColorDrawable(ContextCompat.getColor(requireContext(), R.color.brand_background))
        backgroundManager.drawable = defaultBackground
    }

    private fun setupUIElements() {
        title = "ANDUB"
        headersState = HEADERS_ENABLED
        isHeadersTransitionOnBackEnabled = true
        brandColor = ContextCompat.getColor(requireContext(), R.color.brand_background)
        searchAffordanceColor = ContextCompat.getColor(requireContext(), R.color.brand_accent)

        setHeaderPresenterSelector(object : PresenterSelector() {
            private val iconPresenter = IconRowHeaderPresenter()
            override fun getPresenter(item: Any?): Presenter = iconPresenter
        })

        (titleView as? CustomTitleView)?.apply {
            setProviderName(ProviderRegistry.currentProvider.displayName)
            onProviderClickListener = {
                showProviderChooserDialog()
            }
        }
    }

    private fun showProviderChooserDialog() {
        val providers = ProviderRegistry.getProviders()
        val names = providers.map { it.displayName }.toTypedArray()
        val currentIndex = providers.indexOfFirst {
            it.name.equals(ProviderRegistry.currentProvider.name, ignoreCase = true)
        }.coerceAtLeast(0)

        android.app.AlertDialog.Builder(requireContext(), android.R.style.Theme_DeviceDefault_Dialog_Alert)
            .setTitle("Оберіть джерело (провайдер)")
            .setSingleChoiceItems(names, currentIndex) { dialog, which ->
                dialog.dismiss()
                switchProvider(providers[which])
            }
            .setNegativeButton("Скасувати", null)
            .show()
    }

    private fun setupRows() {
        val listRowPresenter = GridListRowPresenter().apply {
            shadowEnabled = false
            selectEffectEnabled = false
        }
        rowsAdapter = ArrayObjectAdapter(listRowPresenter)
        adapter = rowsAdapter
        rebuildRowStructure()
    }

    private fun rebuildRowStructure() {
        rowsAdapter.clear()
        var headerId = 0L

        // 1. Popular
        rowsAdapter.add(ListRow(IconHeaderItem(headerId++, getString(R.string.header_popular), R.drawable.ic_play), popularAdapter))

        // 2. Latest
        rowsAdapter.add(ListRow(IconHeaderItem(headerId++, getString(R.string.header_latest), R.drawable.ic_anitube), latestAdapter))

        // 3. Favorites (only if not empty)
        if (favoritesAdapter.size() > 0) {
            rowsAdapter.add(ListRow(IconHeaderItem(headerId++, getString(R.string.header_favorites), R.drawable.ic_favorites), favoritesAdapter))
        }

        // 4. History (only if not empty)
        if (historyAdapter.size() > 0) {
            rowsAdapter.add(ListRow(IconHeaderItem(headerId++, getString(R.string.header_history), R.drawable.ic_history), historyAdapter))
        }

        // 5. Genres
        rowsAdapter.add(ListRow(IconHeaderItem(headerId++, "Пошук за жанрами", R.drawable.ic_genres), genresAdapter))

        // 6. Providers
        rowsAdapter.add(ListRow(IconHeaderItem(headerId++, getString(R.string.header_providers), R.drawable.ic_anilibria), providersAdapter))
    }

    private fun setupEventListeners() {
        setOnSearchClickedListener {
            SearchTvActivity.start(requireContext())
        }

        setOnItemViewClickedListener { _, item, _, _ ->
            when (item) {
                is Anime -> {
                    DetailsTvActivity.start(requireContext(), item)
                }
                is BaseProvider -> {
                    switchProvider(item)
                }
                is String -> {
                    SearchTvActivity.startWithGenre(requireContext(), item)
                }
            }
        }

        setOnItemViewSelectedListener { _, item, _, _ ->
            if (item is Anime) {
                updateAmbientBlurBackground(item.posterUrl)
            }
        }
    }

    private fun updateAmbientBlurBackground(posterUrl: String) {
        if (posterUrl.isEmpty()) {
            backgroundManager.drawable = defaultBackground
            return
        }

        Glide.with(requireContext())
            .asBitmap()
            .load(ua.andub.tv.ui.common.GlideUtils.buildGlideUrl(posterUrl))
            .into(object : CustomTarget<Bitmap>(320, 180) {
                override fun onResourceReady(resource: Bitmap, transition: Transition<in Bitmap>?) {
                    lifecycleScope.launch(Dispatchers.Default) {
                        val blurred = BlurUtils.blur(resource, radius = 18, darkTintAlpha = 200)
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

    private fun switchProvider(provider: BaseProvider) {
        ProviderRegistry.setProvider(provider.name)
        (titleView as? CustomTitleView)?.setProviderName(provider.displayName)
        refreshProvidersRow()
        loadAllData()
    }

    private fun refreshProvidersRow() {
        providersAdapter.clear()
        ProviderRegistry.getProviders().forEach { providersAdapter.add(it) }
    }

    private fun refreshLocalRows() {
        val favs = FavoritesManager.getFavorites(requireContext())
        val oldFavSize = favoritesAdapter.size()
        favoritesAdapter.clear()
        favs.forEach { favoritesAdapter.add(it) }

        val hist = HistoryManager.getHistory(requireContext())
        val oldHistSize = historyAdapter.size()
        historyAdapter.clear()
        hist.forEach { historyAdapter.add(it) }

        if ((oldFavSize == 0 && favs.isNotEmpty()) || (oldFavSize > 0 && favs.isEmpty()) ||
            (oldHistSize == 0 && hist.isNotEmpty()) || (oldHistSize > 0 && hist.isEmpty())) {
            rebuildRowStructure()
        }
    }

    private fun normalizeKey(title: String): String {
        return title.lowercase()
            .replace(Regex("""\(.*?\)|\[.*?\]|\{.*?\}"""), "")
            .replace(Regex("""[^a-zA-Z0-9\u0400-\u04FF]"""), "")
            .trim()
    }

    private fun loadAllData() {
        loadDataJob?.cancel()
        refreshProvidersRow()
        refreshLocalRows()

        // Populate genres row once
        if (genresAdapter.size() == 0) {
            ua.andub.tv.ui.search.SearchTvFragment.ALL_GENRES.forEach { genresAdapter.add(it) }
        }

        // Clear existing items to prevent duplicates while loading
        popularAdapter.clear()
        latestAdapter.clear()

        val provider = ProviderRegistry.currentProvider
        val seenKeys = Collections.synchronizedSet(mutableSetOf<String>())
        val seenIds = Collections.synchronizedSet(mutableSetOf<String>())

        fun filterUnique(list: List<Anime>): List<Anime> {
            return list.filter { anime ->
                val norm = normalizeKey(anime.primaryTitle)
                val idKey = anime.id.trim().lowercase()
                val isNew = (norm.isEmpty() || seenKeys.add(norm)) && (idKey.isEmpty() || seenIds.add(idKey))
                anime.id.isNotBlank() && isNew
            }
        }

        loadDataJob = viewLifecycleOwner.lifecycleScope.launch {
            // 1. Popular (Top priority)
            try {
                val list = withContext(Dispatchers.IO) { provider.search(query = "", limit = 20, page = 1) }
                val unique = filterUnique(list)
                popularAdapter.clear()
                unique.take(15).forEach { popularAdapter.add(it) }
            } catch (e: Exception) {
                e.printStackTrace()
            }

            // 2. Latest (Page 2)
            try {
                val list = withContext(Dispatchers.IO) { provider.search(query = "", limit = 20, page = 2) }
                val unique = filterUnique(list)
                latestAdapter.clear()
                unique.take(15).forEach { latestAdapter.add(it) }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }
}

package ua.andub.tv.data.storage

import android.content.Context
import android.content.SharedPreferences
import org.json.JSONArray
import org.json.JSONObject
import ua.andub.tv.data.models.Anime

class FavoritesManager(private val context: Context) {

    fun getFavorites(): List<Anime> = getFavorites(context)
    fun isFavorite(provider: String, id: String): Boolean = isFavorite(context, provider, id)
    fun isFavorite(anime: Anime): Boolean = isFavorite(context, anime)
    fun addFavorite(anime: Anime) = addFavorite(context, anime)
    fun removeFavorite(provider: String, id: String) = removeFavorite(context, provider, id)
    fun removeFavorite(anime: Anime) = removeFavorite(context, anime)

    companion object {
        private const val PREFS_NAME = "andub_favorites"
        private const val KEY_FAVORITES = "favorites_list"

        private fun getPrefs(context: Context): SharedPreferences {
            return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        }

        fun getFavorites(context: Context): List<Anime> {
            val jsonStr = getPrefs(context).getString(KEY_FAVORITES, "[]") ?: "[]"
            val list = mutableListOf<Anime>()
            try {
                val arr = JSONArray(jsonStr)
                for (i in 0 until arr.length()) {
                    val obj = arr.getJSONObject(i)
                    val genresArr = obj.optJSONArray("genres")
                    val genres = mutableListOf<String>()
                    if (genresArr != null) {
                        for (g in 0 until genresArr.length()) {
                            genres.add(genresArr.getString(g))
                        }
                    }
                    list.add(
                        Anime(
                            id = obj.optString("id"),
                            titleRu = obj.optString("title_ru"),
                            titleEn = obj.optString("title_en"),
                            description = obj.optString("description"),
                            year = obj.optInt("year", 0),
                            genres = genres,
                            status = obj.optString("status"),
                            provider = obj.optString("provider"),
                            posterUrl = obj.optString("poster_url"),
                            rating = obj.optString("rating")
                        )
                    )
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
            return list
        }

        fun isFavorite(context: Context, provider: String, id: String): Boolean {
            return getFavorites(context).any { it.provider == provider && it.id == id }
        }

        fun isFavorite(context: Context, anime: Anime): Boolean {
            return isFavorite(context, anime.provider, anime.id)
        }

        fun addFavorite(context: Context, anime: Anime) {
            val current = getFavorites(context).toMutableList()
            if (current.none { it.provider == anime.provider && it.id == anime.id }) {
                current.add(0, anime)
                saveList(context, current)
            }
        }

        fun removeFavorite(context: Context, provider: String, id: String) {
            val current = getFavorites(context).toMutableList()
            current.removeAll { it.provider == provider && it.id == id }
            saveList(context, current)
        }

        fun removeFavorite(context: Context, anime: Anime) {
            removeFavorite(context, anime.provider, anime.id)
        }

        private fun saveList(context: Context, list: List<Anime>) {
            try {
                val arr = JSONArray()
                for (anime in list) {
                    val obj = JSONObject().apply {
                        put("id", anime.id)
                        put("title_ru", anime.titleRu)
                        put("title_en", anime.titleEn)
                        put("description", anime.description)
                        put("year", anime.year)
                        put("genres", JSONArray(anime.genres))
                        put("status", anime.status)
                        put("provider", anime.provider)
                        put("poster_url", anime.posterUrl)
                        put("rating", anime.rating)
                    }
                    arr.put(obj)
                }
                getPrefs(context).edit().putString(KEY_FAVORITES, arr.toString()).apply()
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }
}

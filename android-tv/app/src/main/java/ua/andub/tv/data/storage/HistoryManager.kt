package ua.andub.tv.data.storage

import android.content.Context
import android.content.SharedPreferences
import org.json.JSONArray
import org.json.JSONObject
import ua.andub.tv.data.models.Anime
import java.io.Serializable

data class HistoryEntry(
    val provider: String,
    val animeId: String,
    val animeTitle: String,
    val posterUrl: String,
    val lastEpisode: String,
    val positionMs: Long,
    val durationMs: Long,
    val timestamp: Long
) : Serializable {
    fun toAnime(): Anime = Anime(
        id = animeId,
        titleRu = animeTitle,
        posterUrl = posterUrl,
        provider = provider,
        meta = mapOf("last_episode" to lastEpisode)
    )
}

class HistoryManager(private val context: Context) {

    fun getHistory(): List<Anime> = getHistory(context)
    fun getHistoryEntries(): List<HistoryEntry> = getHistoryEntries(context)
    fun getProgress(provider: String, animeId: String, episodeNum: String): Long =
        getProgress(context, provider, animeId, episodeNum)

    companion object {
        private const val PREFS_NAME = "andub_history"
        private const val KEY_HISTORY = "history_entries"

        private fun getPrefs(context: Context): SharedPreferences {
            return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        }

        fun saveProgress(
            context: Context,
            anime: Anime,
            episodeNum: String,
            positionMs: Long,
            durationMs: Long
        ) {
            saveProgress(
                context,
                anime.provider,
                anime.id,
                episodeNum,
                positionMs,
                durationMs,
                anime.primaryTitle,
                anime.posterUrl
            )
        }

        fun saveProgress(
            context: Context,
            provider: String,
            animeId: String,
            episodeNum: String,
            positionMs: Long,
            durationMs: Long,
            animeTitle: String = "",
            posterUrl: String = ""
        ) {
            val prefs = getPrefs(context)
            val key = "${provider}_${animeId}_${episodeNum}"
            prefs.edit().putLong(key, positionMs).apply()

            // Also update recent entries list
            val list = getHistoryEntries(context).toMutableList()
            list.removeAll { it.provider == provider && it.animeId == animeId }
            list.add(
                0,
                HistoryEntry(
                    provider = provider,
                    animeId = animeId,
                    animeTitle = animeTitle,
                    posterUrl = posterUrl,
                    lastEpisode = episodeNum,
                    positionMs = positionMs,
                    durationMs = durationMs,
                    timestamp = System.currentTimeMillis()
                )
            )
            val trimmed = if (list.size > 50) list.subList(0, 50) else list
            saveHistoryList(context, trimmed)
        }

        fun getProgress(context: Context, animeId: String, episodeNum: String): Long {
            val prefs = getPrefs(context)
            val allKeys = prefs.all.keys
            val suffix = "_${animeId}_${episodeNum}"
            val matchingKey = allKeys.firstOrNull { it.endsWith(suffix) }
            return if (matchingKey != null) {
                prefs.getLong(matchingKey, 0L)
            } else {
                prefs.getLong("${animeId}_${episodeNum}", 0L)
            }
        }

        fun getProgress(context: Context, provider: String, animeId: String, episodeNum: String): Long {
            val key = "${provider}_${animeId}_${episodeNum}"
            return getPrefs(context).getLong(key, 0L)
        }

        fun getHistory(context: Context): List<Anime> {
            return getHistoryEntries(context).map { it.toAnime() }
        }

        fun getHistoryEntries(context: Context): List<HistoryEntry> {
            val jsonStr = getPrefs(context).getString(KEY_HISTORY, "[]") ?: "[]"
            val list = mutableListOf<HistoryEntry>()
            try {
                val arr = JSONArray(jsonStr)
                for (i in 0 until arr.length()) {
                    val obj = arr.getJSONObject(i)
                    list.add(
                        HistoryEntry(
                            provider = obj.optString("provider"),
                            animeId = obj.optString("anime_id"),
                            animeTitle = obj.optString("anime_title"),
                            posterUrl = obj.optString("poster_url"),
                            lastEpisode = obj.optString("last_episode"),
                            positionMs = obj.optLong("position_ms"),
                            durationMs = obj.optLong("duration_ms"),
                            timestamp = obj.optLong("timestamp")
                        )
                    )
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
            return list
        }

        private fun saveHistoryList(context: Context, list: List<HistoryEntry>) {
            try {
                val arr = JSONArray()
                for (item in list) {
                    val obj = JSONObject().apply {
                        put("provider", item.provider)
                        put("anime_id", item.animeId)
                        put("anime_title", item.animeTitle)
                        put("poster_url", item.posterUrl)
                        put("last_episode", item.lastEpisode)
                        put("position_ms", item.positionMs)
                        put("durationMs", item.durationMs)
                        put("timestamp", item.timestamp)
                    }
                    arr.put(obj)
                }
                getPrefs(context).edit().putString(KEY_HISTORY, arr.toString()).apply()
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }
}

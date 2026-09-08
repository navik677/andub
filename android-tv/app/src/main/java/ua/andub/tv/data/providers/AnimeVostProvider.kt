package ua.andub.tv.data.providers

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.FormBody
import okhttp3.Request
import org.json.JSONArray
import org.json.JSONObject
import ua.andub.tv.data.models.Anime
import ua.andub.tv.data.models.Episode
import ua.andub.tv.data.models.Quality
import ua.andub.tv.data.models.Stream
import java.util.regex.Pattern

class AnimeVostProvider : BaseProvider() {

    override val name: String = "AnimeVost"
    override val displayName: String = "AnimeVost"
    private val apiBase = "https://api.animevost.org/v1"

    override suspend fun search(query: String, limit: Int, genre: String, page: Int): List<Anime> = withContext(Dispatchers.IO) {
        val request = if (query.isEmpty()) {
            val url = "$apiBase/last?page=$page&quantity=$limit"
            Request.Builder()
                .url(url)
                .get()
                .build()
        } else {
            val form = FormBody.Builder()
                .add("name", query)
                .build()
            Request.Builder()
                .url("$apiBase/search")
                .post(form)
                .build()
        }

        try {
            NetworkClient.client.newCall(request).execute().use { response ->
                if (!response.isSuccessful) return@withContext emptyList()
                val body = response.body?.string() ?: return@withContext emptyList()
                val root = JSONObject(body)
                val dataArr = root.optJSONArray("data") ?: return@withContext emptyList()

                val results = mutableListOf<Anime>()
                for (i in 0 until dataArr.length()) {
                    val item = dataArr.getJSONObject(i)
                    val a = parseAnimeItem(item)

                    if (genre.isNotEmpty()) {
                        val matches = a.genres.any { it.contains(genre, ignoreCase = true) }
                        if (!matches) continue
                    }

                    results.add(a)
                    if (results.size >= limit) break
                }
                results
            }
        } catch (e: Exception) {
            e.printStackTrace()
            emptyList()
        }
    }

    override suspend fun getEpisodes(anime: Anime): List<Episode> = withContext(Dispatchers.IO) {
        val form = FormBody.Builder()
            .add("id", anime.id)
            .build()

        val request = Request.Builder()
            .url("$apiBase/playlist")
            .post(form)
            .build()

        try {
            NetworkClient.client.newCall(request).execute().use { response ->
                if (!response.isSuccessful) return@withContext emptyList()
                val body = response.body?.string() ?: return@withContext emptyList()
                val root = JSONArray(body)

                val episodes = mutableListOf<Episode>()
                val numPattern = Pattern.compile("(\\d+)")

                for (i in 0 until root.length()) {
                    val item = root.getJSONObject(i)
                    val epName = item.optString("name", "Епізод")

                    val matcher = numPattern.matcher(epName)
                    val number = if (matcher.find()) matcher.group(1) ?: (i + 1).toString() else (i + 1).toString()

                    val hd = item.optString("hd")
                    val stdUrl = item.optString("std")
                    val streamUrls = mutableMapOf<String, String>()
                    if (hd.isNotEmpty()) streamUrls["720p"] = hd
                    if (stdUrl.isNotEmpty()) streamUrls["480p"] = stdUrl

                    episodes.add(
                        Episode(
                            number = number,
                            title = epName,
                            streamUrls = streamUrls
                        )
                    )
                }
                episodes
            }
        } catch (e: Exception) {
            e.printStackTrace()
            emptyList()
        }
    }

    override suspend fun getStream(anime: Anime, episode: Episode): Stream = withContext(Dispatchers.IO) {
        val qualities = episode.streamUrls.map { (label, url) ->
            Quality(
                label = label,
                url = url,
                headers = mapOf(
                    "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
                )
            )
        }
        Stream(qualities = qualities)
    }

    private fun parseAnimeItem(item: JSONObject): Anime {
        val rawId = item.opt("id")?.toString() ?: "0"
        val rawTitle = item.optString("title", "Без назви")

        var titleRu = rawTitle
        var titleEn = ""
        val slash = rawTitle.indexOf('/')
        if (slash != -1) {
            titleRu = rawTitle.substring(0, slash).trim()
            titleEn = rawTitle.substring(slash + 1).trim()
        }

        val yearStr = item.optString("year")
        val year = yearStr.toIntOrNull() ?: item.optInt("year", 0)

        var description = item.optString("description")
        description = description.replace(Regex("<br\\s*/?>", RegexOption.IGNORE_CASE), "\n")

        val genresList = mutableListOf<String>()
        val gVal = item.opt("genre")
        if (gVal is String) {
            gVal.split(",").forEach { g ->
                val trimmed = g.trim()
                if (trimmed.isNotEmpty()) genresList.add(trimmed)
            }
        } else {
            val genresArr = item.optJSONArray("genres")
            if (genresArr != null) {
                for (i in 0 until genresArr.length()) {
                    val g = genresArr.optString(i)
                    if (g.isNotEmpty()) genresList.add(g)
                }
            }
        }

        val ratingInt = item.optInt("rating", 0)
        val votesInt = item.optInt("votes", 0)
        val rating = when {
            ratingInt > 0 -> "★ $ratingInt"
            votesInt > 0 -> "★ $votesInt"
            else -> ""
        }

        val poster = item.optString("urlImagePreview")

        return Anime(
            id = rawId,
            titleRu = titleRu,
            titleEn = titleEn,
            year = year,
            description = description,
            genres = genresList,
            rating = rating,
            posterUrl = poster,
            provider = name
        )
    }
}

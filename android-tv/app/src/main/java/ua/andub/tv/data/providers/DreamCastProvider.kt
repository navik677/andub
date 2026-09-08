package ua.andub.tv.data.providers

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.Request
import org.json.JSONArray
import org.json.JSONObject
import ua.andub.tv.data.models.Anime
import ua.andub.tv.data.models.Episode
import ua.andub.tv.data.models.Stream
import java.net.URLEncoder

class DreamCastProvider : BaseProvider() {
    override val name: String = "dreamcast"
    override val displayName: String = "Dream Cast"

    private val apiBase = "https://kodik-api.com"
    private val apiToken = "56a768d08f43091901c44b54fe970049"
    private val translationId = 1978

    override suspend fun search(
        query: String,
        limit: Int,
        genre: String,
        page: Int
    ): List<Anime> = withContext(Dispatchers.IO) {
        val results = mutableListOf<Anime>()
        try {
            val url = if (query.isBlank()) {
                "$apiBase/list?token=$apiToken&translation_id=$translationId&types=anime-serial,anime&with_material_data=true&limit=$limit"
            } else {
                "$apiBase/search?token=$apiToken&translation_id=$translationId&title=${URLEncoder.encode(query, "UTF-8")}&types=anime-serial,anime&with_material_data=true&limit=$limit"
            }

            val req = Request.Builder()
                .url(url)
                .header("User-Agent", NetworkClient.USER_AGENT)
                .build()

            val resp = NetworkClient.client.newCall(req).execute()
            val body = resp.body?.string() ?: ""

            val root = JSONObject(body)
            val resArr = root.optJSONArray("results") ?: JSONArray()

            for (i in 0 until resArr.length()) {
                val item = resArr.getJSONObject(i)
                val id = item.optString("id", "")
                val titleRu = item.optString("title", "")
                val titleEn = item.optString("title_orig", "")
                val year = item.optInt("year", 0)
                var link = item.optString("link", "")
                if (link.startsWith("//")) link = "https:$link"

                val epsCount = item.optInt("episodes_count", 1)

                val md = item.optJSONObject("material_data")
                var desc = md?.optString("description", "") ?: ""
                if (desc.isBlank()) desc = md?.optString("anime_description", "") ?: ""

                var poster = md?.optString("anime_poster_url", "") ?: ""
                if (poster.isBlank()) poster = md?.optString("poster_url", "") ?: ""
                if (poster.startsWith("//")) poster = "https:$poster"

                val ratingVal = md?.opt("shikimori_rating")
                val rating = if (ratingVal != null) "★ $ratingVal" else ""

                val genresList = mutableListOf<String>()
                val gArr = md?.optJSONArray("anime_genres") ?: md?.optJSONArray("all_genres")
                if (gArr != null) {
                    for (g in 0 until gArr.length()) {
                        val gn = gArr.optString(g)
                        if (gn.isNotBlank()) genresList.add(gn)
                    }
                }

                results.add(
                    Anime(
                        id = id,
                        titleRu = titleRu.ifEmpty { titleEn },
                        titleEn = titleEn,
                        year = year,
                        description = desc,
                        rating = rating,
                        genres = genresList,
                        provider = name,
                        posterUrl = poster,
                        meta = mapOf(
                            "link" to link,
                            "episodes_count" to epsCount.toString()
                        )
                    )
                )
                if (results.size >= limit) break
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        results
    }

    override suspend fun getEpisodes(anime: Anime): List<Episode> = withContext(Dispatchers.IO) {
        var kodikUrl = anime.meta["link"] ?: ""
        var epsCount = anime.meta["episodes_count"]?.toIntOrNull() ?: 1

        if (kodikUrl.isEmpty()) {
            val searchResults = search(anime.primaryTitle, limit = 5)
            val matched = searchResults.firstOrNull()
            if (matched != null) {
                kodikUrl = matched.meta["link"] ?: ""
                epsCount = matched.meta["episodes_count"]?.toIntOrNull() ?: epsCount
            }
        }

        if (kodikUrl.isEmpty()) {
            return@withContext listOf(Episode("1", "Серія 1", meta = mapOf("kodik_url" to "", "episode_num" to "1")))
        }

        val episodes = mutableListOf<Episode>()

        // Try discovering exact episodes from Kodik page
        val epNums = KodikResolver.getEpisodes(kodikUrl)
        if (epNums.isNotEmpty()) {
            for (num in epNums) {
                episodes.add(
                    Episode(
                        number = num.toString(),
                        title = "Серія $num",
                        meta = mapOf("kodik_url" to kodikUrl, "episode_num" to num.toString())
                    )
                )
            }
            return@withContext episodes
        }

        for (i in 1..epsCount) {
            episodes.add(
                Episode(
                    number = i.toString(),
                    title = "Серія $i",
                    meta = mapOf("kodik_url" to kodikUrl, "episode_num" to i.toString())
                )
            )
        }

        episodes
    }

    override suspend fun getStream(anime: Anime, episode: Episode): Stream = withContext(Dispatchers.IO) {
        var kodikUrl = episode.meta["kodik_url"] ?: anime.meta["link"] ?: ""
        if (kodikUrl.isEmpty()) {
            val searchResults = search(anime.primaryTitle, limit = 5)
            val matched = searchResults.firstOrNull()
            kodikUrl = matched?.meta?.get("link") ?: ""
        }
        if (kodikUrl.isEmpty()) return@withContext Stream()

        val epNum = episode.meta["episode_num"]?.toIntOrNull() ?: episode.number.toIntOrNull() ?: 1
        KodikResolver.resolve(kodikUrl, epNum)
    }
}

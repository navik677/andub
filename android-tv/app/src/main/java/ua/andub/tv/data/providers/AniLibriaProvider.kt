package ua.andub.tv.data.providers

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.Request
import org.json.JSONArray
import org.json.JSONObject
import ua.andub.tv.data.models.Anime
import ua.andub.tv.data.models.Episode
import ua.andub.tv.data.models.Quality
import ua.andub.tv.data.models.Stream
import java.net.URLEncoder

class AniLibriaProvider : BaseProvider() {
    override val name: String = "anilibria"
    override val displayName: String = "АніЛібрія"

    private val apiBase = "https://anilibria.top/api/v1"
    private val streamHost = "https://cache.libria.fun"

    override suspend fun search(
        query: String,
        limit: Int,
        genre: String,
        page: Int
    ): List<Anime> = withContext(Dispatchers.IO) {
        val results = mutableListOf<Anime>()
        try {
            var url = "$apiBase/anime/catalog/releases?page=$page&limit=$limit"
            if (query.isNotBlank()) {
                url += "&f[search]=${URLEncoder.encode(query, "UTF-8")}"
            }

            val req = Request.Builder()
                .url(url)
                .header("User-Agent", NetworkClient.USER_AGENT)
                .build()

            val resp = NetworkClient.client.newCall(req).execute()
            val body = resp.body?.string() ?: ""

            val root = JSONObject(body)
            val dataArr = root.optJSONArray("data") ?: JSONArray()

            for (i in 0 until dataArr.length()) {
                val item = dataArr.getJSONObject(i)
                val id = item.optString("id", "")
                val names = item.optJSONObject("name") ?: item.optJSONObject("names")
                val titleRu = names?.optString("main", "")?.ifEmpty { names.optString("ru", "") } ?: ""
                val titleEn = names?.optString("english", "")?.ifEmpty { names.optString("en", "") } ?: ""
                val desc = item.optString("description", "")
                val year = item.optInt("year", 0)

                val posterObj = item.optJSONObject("poster")
                var posterUrl = ""
                if (posterObj != null) {
                    val src = posterObj.optString("src", "")
                    if (src.isNotBlank()) {
                        posterUrl = if (src.startsWith("http")) src else "https://anilibria.top$src"
                    }
                }

                val genresList = mutableListOf<String>()
                val genresArr = item.optJSONArray("genres")
                if (genresArr != null) {
                    for (g in 0 until genresArr.length()) {
                        val gObj = genresArr.optJSONObject(g)
                        val gName = gObj?.optString("name") ?: genresArr.optString(g)
                        if (gName.isNotBlank()) genresList.add(gName)
                    }
                }

                results.add(
                    Anime(
                        id = id,
                        titleRu = titleRu.ifEmpty { "Без назви" },
                        titleEn = titleEn,
                        description = desc,
                        year = year,
                        genres = genresList,
                        provider = name,
                        posterUrl = posterUrl
                    )
                )
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        results
    }

    override suspend fun getEpisodes(anime: Anime): List<Episode> = withContext(Dispatchers.IO) {
        val episodes = mutableListOf<Episode>()
        try {
            val url = "$apiBase/anime/releases/${anime.id}"
            val req = Request.Builder()
                .url(url)
                .header("User-Agent", NetworkClient.USER_AGENT)
                .build()
            val resp = NetworkClient.client.newCall(req).execute()
            val body = resp.body?.string() ?: ""

            val root = JSONObject(body)
            val epsArr = root.optJSONArray("episodes")

            if (epsArr != null && epsArr.length() > 0) {
                for (i in 0 until epsArr.length()) {
                    val epObj = epsArr.getJSONObject(i)
                    val ord = epObj.optDouble("ordinal", (i + 1).toDouble())
                    val numStr = if (ord == ord.toLong().toDouble()) ord.toLong().toString() else ord.toString()
                    val epName = epObj.optString("name", "")

                    val streamMap = mutableMapOf<String, String>()
                    val hls1080 = epObj.optString("hls_1080", "")
                    val hls720 = epObj.optString("hls_720", "")
                    val hls480 = epObj.optString("hls_480", "")

                    if (hls1080.isNotBlank()) streamMap["1080p"] = formatStreamUrl(hls1080)
                    if (hls720.isNotBlank()) streamMap["720p"] = formatStreamUrl(hls720)
                    if (hls480.isNotBlank()) streamMap["480p"] = formatStreamUrl(hls480)

                    val epTitle = if (epName.isNotBlank() && epName != "null" && epName != "None") "Серія $numStr — $epName" else "Серія $numStr"

                    episodes.add(
                        Episode(
                            number = numStr,
                            title = epTitle,
                            streamUrls = streamMap,
                            meta = mapOf("id" to epObj.optString("id"))
                        )
                    )
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        episodes
    }

    override suspend fun getStream(anime: Anime, episode: Episode): Stream = withContext(Dispatchers.IO) {
        val qualities = mutableListOf<Quality>()
        val defaultHeaders = mapOf(
            "User-Agent" to NetworkClient.USER_AGENT,
            "Referer" to "https://anilibria.top/"
        )

        val order = listOf("1080p", "720p", "480p")
        for (qLabel in order) {
            val url = episode.streamUrls[qLabel]
            if (!url.isNullOrBlank()) {
                qualities.add(Quality(label = qLabel, url = url, headers = defaultHeaders))
            }
        }

        val primary = qualities.firstOrNull()?.url ?: ""
        Stream(
            url = primary,
            quality = qualities.firstOrNull()?.label ?: "auto",
            headers = defaultHeaders,
            qualities = qualities
        )
    }

    private fun formatStreamUrl(url: String): String {
        return if (url.startsWith("http")) url else if (url.startsWith("/")) "$streamHost$url" else "$streamHost/$url"
    }
}

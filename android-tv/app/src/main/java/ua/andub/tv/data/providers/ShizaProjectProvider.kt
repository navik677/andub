package ua.andub.tv.data.providers

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import ua.andub.tv.data.models.Anime
import ua.andub.tv.data.models.Episode
import ua.andub.tv.data.models.Quality
import ua.andub.tv.data.models.Stream

class ShizaProjectProvider : BaseProvider() {

    override val name: String = "ShizaProject"
    override val displayName: String = "Shiza Project"
    private val graphqlUrl = "https://shizaproject.com/graphql"

    override suspend fun search(query: String, limit: Int, genre: String, page: Int): List<Anime> = withContext(Dispatchers.IO) {
        val fetchCount = if (page > 1) limit * page else limit
        val gqlQuery = if (query.isNotEmpty()) {
            """query { releases(query: "$query", first: $fetchCount) { edges { node { id name originalName slug description posters { preview: resize(width: 360, height: 500) { url } original { url } } genres { name } } } } }"""
        } else {
            """query { releases(first: $fetchCount) { edges { node { id name originalName slug description posters { preview: resize(width: 360, height: 500) { url } original { url } } genres { name } } } } }"""
        }

        val jsonBody = JSONObject().apply { put("query", gqlQuery) }
        val requestBody = jsonBody.toString().toRequestBody("application/json".toMediaType())

        val request = Request.Builder()
            .url(graphqlUrl)
            .post(requestBody)
            .header("Content-Type", "application/json")
            .header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36")
            .build()

        try {
            NetworkClient.client.newCall(request).execute().use { response ->
                if (!response.isSuccessful) return@withContext emptyList()
                val body = response.body?.string() ?: return@withContext emptyList()
                val root = JSONObject(body)
                val edges = root.optJSONObject("data")
                    ?.optJSONObject("releases")
                    ?.optJSONArray("edges") ?: return@withContext emptyList()

                val results = mutableListOf<Anime>()
                val lowerGenre = genre.lowercase()
                val skipCount = if (page > 1) (page - 1) * limit else 0
                var matchedCount = 0

                for (i in 0 until edges.length()) {
                    val node = edges.getJSONObject(i).getJSONObject("node")
                    val id = node.optString("slug").ifEmpty { node.optString("id") }
                    val nameRu = node.optString("name")
                    val nameEn = node.optString("originalName")
                    val desc = node.optString("description")
                    val slug = node.optString("slug")

                    var posterUrl = ""
                    val posters = node.optJSONArray("posters")
                    if (posters != null && posters.length() > 0) {
                        val p0 = posters.getJSONObject(0)
                        posterUrl = p0.optJSONObject("preview")?.optString("url") ?: ""
                        if (posterUrl.isEmpty()) {
                            posterUrl = p0.optJSONObject("original")?.optString("url") ?: ""
                        }
                    }

                    val genresList = mutableListOf<String>()
                    val genresArr = node.optJSONArray("genres")
                    var matchesGenre = genre.isEmpty()

                    if (genresArr != null) {
                        for (g in 0 until genresArr.length()) {
                            val gname = genresArr.getJSONObject(g).optString("name")
                            if (gname.isNotEmpty()) {
                                genresList.add(gname)
                                if (!matchesGenre && gname.lowercase().contains(lowerGenre)) {
                                    matchesGenre = true
                                }
                            }
                        }
                    }

                    if (!matchesGenre) continue

                    matchedCount++
                    if (matchedCount <= skipCount) continue

                    results.add(
                        Anime(
                            id = id,
                            titleRu = nameRu,
                            titleEn = nameEn,
                            description = desc,
                            posterUrl = posterUrl,
                            genres = genresList,
                            provider = name,
                            meta = mapOf("slug" to slug)
                        )
                    )
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
        val slug = anime.meta["slug"]?.ifEmpty { anime.id } ?: anime.id
        val gqlQuery = """query { release(slug: "$slug") { episodes { id number name videos { id embedSource embedUrl } } } }"""

        val jsonBody = JSONObject().apply { put("query", gqlQuery) }
        val requestBody = jsonBody.toString().toRequestBody("application/json".toMediaType())

        val request = Request.Builder()
            .url(graphqlUrl)
            .post(requestBody)
            .header("Content-Type", "application/json")
            .header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36")
            .build()

        try {
            NetworkClient.client.newCall(request).execute().use { response ->
                if (!response.isSuccessful) return@withContext emptyList()
                val body = response.body?.string() ?: return@withContext emptyList()
                val root = JSONObject(body)
                val epsArr = root.optJSONObject("data")
                    ?.optJSONObject("release")
                    ?.optJSONArray("episodes") ?: return@withContext emptyList()

                val episodes = mutableListOf<Episode>()
                for (i in 0 until epsArr.length()) {
                    val item = epsArr.getJSONObject(i)
                    val num = item.optInt("number", i + 1)
                    val epTitle = item.optString("name")
                    val vids = item.optJSONArray("videos")
                    val streamUrls = mutableMapOf<String, String>()
                    val metaMap = mutableMapOf<String, String>()

                    if (vids != null) {
                        for (v in 0 until vids.length()) {
                            val vObj = vids.getJSONObject(v)
                            var src = vObj.optString("embedSource")
                            val url = vObj.optString("embedUrl")
                            if (url.isNotEmpty()) {
                                if (src.isEmpty()) src = "video_$v"
                                streamUrls[src] = url
                                if (!metaMap.containsKey("default_url") || src == "KODIK") {
                                    metaMap["default_url"] = url
                                }
                            }
                        }
                    }

                    episodes.add(
                        Episode(
                            number = num.toString(),
                            title = epTitle.ifEmpty { "Серія $num" },
                            streamUrls = streamUrls,
                            meta = metaMap
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
        var streamUrl = episode.meta["default_url"]
            ?: episode.streamUrls.values.firstOrNull()

        if (streamUrl.isNullOrEmpty()) {
            try {
                val eps = getEpisodes(anime)
                val matched = eps.firstOrNull { it.number == episode.number } ?: eps.firstOrNull()
                streamUrl = matched?.meta?.get("default_url") ?: matched?.streamUrls?.values?.firstOrNull()
            } catch (e: Exception) {}
        }

        if (streamUrl.isNullOrEmpty()) return@withContext Stream()

        if (streamUrl.contains("kodik")) {
            val epNum = episode.number.toIntOrNull() ?: 1
            val resolved = KodikResolver.resolve(streamUrl, epNum, referer = "https://shizaproject.com/")
            if (resolved.qualities.isNotEmpty()) {
                return@withContext resolved
            }
        }

        Stream(
            qualities = listOf(
                Quality(
                    label = "auto",
                    url = streamUrl,
                    headers = mapOf(
                        "Referer" to "https://shizaproject.com/",
                        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
                    )
                )
            )
        )
    }
}

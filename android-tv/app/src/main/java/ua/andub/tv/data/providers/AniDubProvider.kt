package ua.andub.tv.data.providers

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.FormBody
import okhttp3.HttpUrl.Companion.toHttpUrlOrNull
import okhttp3.Request
import org.jsoup.Jsoup
import ua.andub.tv.data.models.Anime
import ua.andub.tv.data.models.Episode
import ua.andub.tv.data.models.Quality
import ua.andub.tv.data.models.Stream
import java.util.regex.Pattern

class AniDubProvider : BaseProvider() {

    override val name: String = "AniDub"
    override val displayName: String = "AniDub"
    private val baseUrl = "https://anidub.com"

    override suspend fun search(query: String, limit: Int, genre: String, page: Int): List<Anime> = withContext(Dispatchers.IO) {
        val searchQuery = query.ifEmpty { genre.ifEmpty { "аниме" } }
        val formBuilder = FormBody.Builder()
            .add("do", "search")
            .add("subaction", "search")
            .add("story", searchQuery)
        if (page > 1) {
            formBuilder.add("search_start", page.toString())
        }

        val request = Request.Builder()
            .url("$baseUrl/index.php?do=search")
            .post(formBuilder.build())
            .header("Referer", "$baseUrl/")
            .header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36")
            .build()

        try {
            NetworkClient.client.newCall(request).execute().use { response ->
                if (!response.isSuccessful) return@withContext emptyList()
                val html = response.body?.string() ?: return@withContext emptyList()
                parseSearchHtml(html, limit)
            }
        } catch (e: Exception) {
            e.printStackTrace()
            emptyList()
        }
    }

    private fun parseSearchHtml(html: String, limit: Int): List<Anime> {
        val results = mutableListOf<Anime>()
        val doc = Jsoup.parse(html)
        val items = doc.select("a.th-in")

        for (item in items) {
            val href = item.attr("href")
            val titleDiv = item.selectFirst(".th-title")?.text()?.trim() ?: ""
            if (titleDiv.isEmpty()) continue

            val imgEl = item.selectFirst("img")
            var img = imgEl?.attr("data-src")?.ifEmpty { null }
                ?: imgEl?.attr("data-original")?.ifEmpty { null }
                ?: imgEl?.attr("src") ?: ""
            if (img.startsWith("data:") || img.contains("spacer.gif")) {
                img = imgEl?.attr("data-src") ?: ""
            }
            val posterUrl = if (img.isEmpty() || img.startsWith("http")) img else {
                if (img.startsWith("/")) "$baseUrl$img" else "$baseUrl/$img"
            }

            var titleRu = titleDiv
            var titleEn = ""
            val slashIndex = titleDiv.indexOf('/')
            if (slashIndex != -1) {
                titleRu = titleDiv.substring(0, slashIndex).trim()
                titleEn = titleDiv.substring(slashIndex + 1).trim()
            }

            results.add(
                Anime(
                    id = href,
                    titleRu = titleRu,
                    titleEn = titleEn,
                    posterUrl = posterUrl,
                    provider = name,
                    meta = mapOf("url" to href)
                )
            )
            if (results.size >= limit) break
        }
        return results
    }

    override suspend fun getEpisodes(anime: Anime): List<Episode> = withContext(Dispatchers.IO) {
        val pageUrl = anime.meta["url"] ?: anime.id
        val request = Request.Builder()
            .url(pageUrl)
            .header("Referer", "$baseUrl/")
            .header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36")
            .build()

        try {
            NetworkClient.client.newCall(request).execute().use { response ->
                if (!response.isSuccessful) return@withContext emptyList()
                val html = response.body?.string() ?: return@withContext emptyList()
                val episodes = mutableListOf<Episode>()
                val doc = Jsoup.parse(html)

                // 1. Check for episode spans: e.g. <span data="...">Серия X</span>
                val spans = doc.select(".fplayer span[data], .tabs-box span[data]")
                var epIdx = 1

                for (span in spans) {
                    var dataUrl = span.attr("data").trim()
                    val spanText = span.text().trim()
                    if (dataUrl.isEmpty()) continue
                    if (dataUrl.startsWith("//")) dataUrl = "https:$dataUrl"

                    // Ignore master playlist tabs if there are specific episode tabs
                    if (spanText.contains("ПЛЕЕР", ignoreCase = true) && spans.size > 1) {
                        continue
                    }

                    val title = spanText.ifEmpty { "Серія $epIdx" }
                    val streamUrls = mutableMapOf<String, String>()
                    val metaMap = mutableMapOf("page_url" to pageUrl)

                    if (dataUrl.contains("sibnet.ru")) {
                        streamUrls["sibnet"] = dataUrl
                        metaMap["sibnet_url"] = dataUrl
                    } else {
                        streamUrls["player"] = dataUrl
                        metaMap["player_url"] = dataUrl
                    }

                    episodes.add(
                        Episode(
                            number = (epIdx++).toString(),
                            title = title,
                            streamUrls = streamUrls,
                            meta = metaMap
                        )
                    )
                }

                // 2. Fallback to iframe if no episode spans found
                if (episodes.isEmpty()) {
                    val iframe = doc.selectFirst("iframe#playeriframe, iframe[src*=sibnet], iframe[src*=ladonyvesna], iframe[src*=player]")
                    var src = iframe?.attr("src") ?: ""
                    if (src.startsWith("//")) src = "https:$src"

                    if (src.isNotEmpty()) {
                        val streamUrls = mutableMapOf<String, String>()
                        val metaMap = mutableMapOf("page_url" to pageUrl)

                        if (src.contains("sibnet.ru")) {
                            streamUrls["sibnet"] = src
                            metaMap["sibnet_url"] = src
                        } else {
                            streamUrls["player"] = src
                            metaMap["player_url"] = src
                        }

                        episodes.add(
                            Episode(
                                number = "1",
                                title = "Серія 1",
                                streamUrls = streamUrls,
                                meta = metaMap
                            )
                        )
                    }
                }

                if (episodes.isEmpty()) {
                    episodes.add(Episode(number = "1", title = "Серія 1", meta = mapOf("page_url" to pageUrl)))
                }
                episodes
            }
        } catch (e: Exception) {
            e.printStackTrace()
            listOf(Episode(number = "1", title = "Серія 1", meta = mapOf("page_url" to pageUrl)))
        }
    }

    override suspend fun getStream(anime: Anime, episode: Episode): Stream = withContext(Dispatchers.IO) {
        val qualities = mutableListOf<Quality>()
        var sibnetUrl = episode.meta["sibnet_url"] ?: episode.streamUrls["sibnet"]
        var playerUrl = episode.meta["player_url"] ?: episode.streamUrls["player"]

        if (sibnetUrl.isNullOrEmpty() && playerUrl.isNullOrEmpty()) {
            try {
                val eps = getEpisodes(anime)
                val matched = eps.firstOrNull { it.number == episode.number } ?: eps.firstOrNull()
                if (matched != null) {
                    sibnetUrl = matched.meta["sibnet_url"] ?: matched.streamUrls["sibnet"]
                    playerUrl = matched.meta["player_url"] ?: matched.streamUrls["player"]
                }
            } catch (e: Exception) {}
        }

        // 1. Try resolving Sibnet direct MP4 stream
        if (!sibnetUrl.isNullOrEmpty()) {
            try {
                val req = Request.Builder()
                    .url(sibnetUrl)
                    .header("Referer", "$baseUrl/")
                    .header("User-Agent", NetworkClient.USER_AGENT)
                    .build()

                NetworkClient.client.newCall(req).execute().use { resp ->
                    if (resp.isSuccessful) {
                        val body = resp.body?.string() ?: ""
                        val mp4Pattern = Pattern.compile("(/v/[a-zA-Z0-9_/]+\\.mp4)")
                        val m = mp4Pattern.matcher(body)
                        if (m.find()) {
                            val initialMp4 = "https://video.sibnet.ru" + m.group(1)
                            // Resolve 302 redirects to direct dvXX.sibnet.ru video URL
                            val directMp4 = resolveSibnetRedirects(initialMp4)
                            if (directMp4.isNotEmpty()) {
                                qualities.add(
                                    Quality(
                                        label = "720p",
                                        url = directMp4,
                                        headers = mapOf(
                                            "Referer" to "https://video.sibnet.ru/",
                                            "User-Agent" to NetworkClient.USER_AGENT
                                        )
                                    )
                                )
                            }
                        }
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }

        // 2. Try resolving Ladonyvesna / custom player HLS stream
        if (qualities.isEmpty() && !playerUrl.isNullOrEmpty()) {
            try {
                val req = Request.Builder()
                    .url(playerUrl)
                    .header("Referer", "$baseUrl/")
                    .header("User-Agent", NetworkClient.USER_AGENT)
                    .build()

                NetworkClient.client.newCall(req).execute().use { resp ->
                    if (resp.isSuccessful) {
                        val body = resp.body?.string() ?: ""
                        val doc = Jsoup.parse(body)
                        var vSource = doc.selectFirst("source#vsource, source[src*=m3u8], source[src*=vid.php]")?.attr("src") ?: ""
                        if (vSource.isNotEmpty()) {
                            if (vSource.startsWith("//")) {
                                vSource = "https:$vSource"
                            } else if (!vSource.startsWith("http")) {
                                val baseUri = playerUrl.toHttpUrlOrNull()
                                if (baseUri != null) {
                                    vSource = baseUri.resolve(vSource)?.toString() ?: vSource
                                }
                            }

                            qualities.add(
                                Quality(
                                    label = "HLS",
                                    url = vSource,
                                    headers = mapOf(
                                        "Referer" to playerUrl,
                                        "User-Agent" to NetworkClient.USER_AGENT
                                    )
                                )
                            )
                        }
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }

        if (qualities.isNotEmpty()) {
            Stream(
                url = qualities.first().url,
                quality = qualities.first().label,
                headers = qualities.first().headers,
                qualities = qualities
            )
        } else {
            Stream()
        }
    }

    private fun resolveSibnetRedirects(initialUrl: String): String {
        var current = initialUrl
        val noRedirectClient = NetworkClient.client.newBuilder()
            .followRedirects(false)
            .followSslRedirects(false)
            .build()

        for (hop in 0 until 5) {
            try {
                val req = Request.Builder()
                    .url(current)
                    .header("User-Agent", NetworkClient.USER_AGENT)
                    .header("Referer", "https://video.sibnet.ru/")
                    .build()

                noRedirectClient.newCall(req).execute().use { resp ->
                    if (resp.isRedirect) {
                        var loc = resp.header("Location") ?: return current
                        if (loc.startsWith("//")) loc = "https:$loc"
                        current = loc
                    } else if (resp.isSuccessful) {
                        return current
                    } else {
                        return current
                    }
                }
            } catch (e: Exception) {
                break
            }
        }
        return current
    }
}

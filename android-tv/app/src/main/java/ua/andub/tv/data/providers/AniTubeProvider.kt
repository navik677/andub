package ua.andub.tv.data.providers

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.FormBody
import okhttp3.Request
import org.json.JSONObject
import org.jsoup.Jsoup
import ua.andub.tv.data.models.Anime
import ua.andub.tv.data.models.Episode
import ua.andub.tv.data.models.Quality
import ua.andub.tv.data.models.Stream
import java.util.regex.Pattern

class AniTubeProvider : BaseProvider() {
    override val name: String = "AniTube"
    override val displayName: String = "AniTube (UA)"

    private val baseUrl = "https://anitube.in.ua"

    override suspend fun search(
        query: String,
        limit: Int,
        genre: String,
        page: Int
    ): List<Anime> = withContext(Dispatchers.IO) {
        val results = mutableListOf<Anime>()
        try {
            val html = if (query.isNotBlank()) {
                val formBody = FormBody.Builder()
                    .add("do", "search")
                    .add("subaction", "search")
                    .add("search_start", page.toString())
                    .add("full_search", "0")
                    .add("result_from", ((page - 1) * limit + 1).toString())
                    .add("story", query)
                    .build()

                val req = Request.Builder()
                    .url("$baseUrl/index.php?do=search")
                    .post(formBody)
                    .header("User-Agent", NetworkClient.USER_AGENT)
                    .header("Referer", "$baseUrl/")
                    .build()
                NetworkClient.client.newCall(req).execute().body?.string() ?: ""
            } else {
                val url = if (genre.isNotBlank()) {
                    val genreSlug = mapGenreToSlug(genre)
                    if (page > 1) "$baseUrl/anime/$genreSlug/page/$page/" else "$baseUrl/anime/$genreSlug/"
                } else {
                    if (page > 1) "$baseUrl/anime/page/$page/" else "$baseUrl/anime/"
                }
                val req = Request.Builder()
                    .url(url)
                    .header("User-Agent", NetworkClient.USER_AGENT)
                    .header("Referer", "$baseUrl/")
                    .build()
                NetworkClient.client.newCall(req).execute().body?.string() ?: ""
            }

            val doc = Jsoup.parse(html)
            val articles = doc.select("article.story")

            for (art in articles) {
                val linkEl = art.selectFirst("h2 a, h3 a, a.story__title") ?: art.selectFirst("a[href*=-]") ?: continue
                val href = linkEl.attr("href")
                val title = linkEl.text().trim()
                if (title.isEmpty()) continue

                val idMatch = Regex("""/([0-9]+)-""").find(href)
                val id = idMatch?.groupValues?.get(1) ?: href

                val imgEl = art.selectFirst(".story_post img, .story_c_l img, img[data-src]")
                    ?: art.selectFirst("img")
                var poster = imgEl?.attr("data-src")?.ifEmpty { null }
                    ?: imgEl?.attr("data-original")?.ifEmpty { null }
                    ?: imgEl?.attr("src") ?: ""
                if (poster.contains("spacer.gif") || poster.contains("data:image")) {
                    poster = imgEl?.attr("data-src") ?: ""
                }
                if (poster.isNotEmpty() && !poster.startsWith("http")) {
                    poster = if (poster.startsWith("/")) "$baseUrl$poster" else "$baseUrl/$poster"
                }

                val yearEl = art.selectFirst("a[href*=/year/]")
                val year = yearEl?.text()?.toIntOrNull() ?: 0

                val descEl = art.selectFirst(".story__text, .story__content, .story-text")
                val desc = descEl?.text()?.trim() ?: ""

                val genreEls = art.select("a[href*=/genre/]")
                val genres = genreEls.map { it.text().trim() }.filter { it.isNotEmpty() }

                results.add(
                    Anime(
                        id = id,
                        titleRu = title,
                        year = year,
                        description = desc,
                        genres = genres,
                        provider = name,
                        posterUrl = poster,
                        meta = mapOf("url" to href)
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
        val episodes = mutableListOf<Episode>()
        val animeUrl = anime.meta["url"] ?: "$baseUrl/anime/${anime.id}.html"

        try {
            val req = Request.Builder()
                .url(animeUrl)
                .header("User-Agent", NetworkClient.USER_AGENT)
                .header("Referer", "$baseUrl/")
                .build()
            val resp = NetworkClient.client.newCall(req).execute()
            val html = resp.body?.string() ?: ""

            var newsId = ""
            val idMatch = Regex("""data-news_id="(\d+)"""").find(html)
                ?: Regex("""anitube\.in\.ua/(\d+)-""").find(animeUrl)
                ?: Regex("""/(\d+)-""").find(animeUrl)
            if (idMatch != null) newsId = idMatch.groupValues[1]

            var dleHash = ""
            val hashMatch = Regex("""dle_login_hash\s*=\s*['"]([a-f0-9]+)['"]""").find(html)
            if (hashMatch != null) dleHash = hashMatch.groupValues[1]

            if (newsId.isNotBlank()) {
                val formBuilder = FormBody.Builder()
                    .add("news_id", newsId)
                    .add("xfield", "playlist")
                if (dleHash.isNotBlank()) {
                    formBuilder.add("user_hash", dleHash)
                }

                val plReq = Request.Builder()
                    .url("$baseUrl/engine/ajax/playlists.php")
                    .post(formBuilder.build())
                    .header("User-Agent", NetworkClient.USER_AGENT)
                    .header("X-Requested-With", "XMLHttpRequest")
                    .header("Referer", animeUrl)
                    .build()

                val plResp = NetworkClient.client.newCall(plReq).execute()
                val jsonStr = plResp.body?.string() ?: ""

                var htmlResp = jsonStr
                try {
                    val root = JSONObject(jsonStr)
                    htmlResp = root.optString("response").ifEmpty { jsonStr }
                } catch (e: Exception) {}

                val plDoc = Jsoup.parse(htmlResp)

                // Parse category names (0_1 = Озвучення, 0_0 = Субтитри, 0_1_0 = Студія, etc.)
                val catNames = mutableMapOf<String, String>()
                for (li in plDoc.select("li[data-id]")) {
                    val cId = li.attr("data-id").trim()
                    val cName = li.text().trim()
                    if (cId.isNotEmpty() && !li.hasAttr("data-file")) {
                        catNames[cId] = cName
                    }
                }

                data class RawVid(
                    val isVoice: Boolean,
                    val studio: String,
                    val title: String,
                    val url: String,
                    val epNum: Int
                )
                val rawList = mutableListOf<RawVid>()

                // Robust Jsoup query for all video entries: li with data-file attribute
                val videoElements = plDoc.select("li[data-file]")
                for (li in videoElements) {
                    val fileUrl = li.attr("data-file").trim()
                    val dataId = li.attr("data-id").trim()
                    val rawTitle = li.text().trim()
                    if (fileUrl.isEmpty()) continue

                    val parts = dataId.split("_")
                    val typeId = if (parts.size >= 2) "${parts[0]}_${parts[1]}" else ""
                    val studioId = if (parts.size >= 3) "${parts[0]}_${parts[1]}_${parts[2]}" else ""

                    var isVoice = (typeId == "0_1")
                    val typeName = catNames[typeId]?.lowercase() ?: ""
                    if (typeName.contains("озвуч")) isVoice = true
                    else if (typeName.contains("субтит")) isVoice = false

                    val studioName = catNames[studioId] ?: ""
                    val numMatch = Regex("""\d+""").find(rawTitle)
                    val epNum = numMatch?.value?.toIntOrNull() ?: 0

                    rawList.add(RawVid(isVoice, studioName, rawTitle, fileUrl, epNum))
                }

                // Per-studio filtering: if a studio has ashdi, prefer ashdi for that studio, otherwise keep available players
                val ashdiStudios = rawList.filter { it.url.contains("ashdi.vip") }.map { it.studio }.toSet()
                val filteredList = rawList.filter { item ->
                    if (ashdiStudios.contains(item.studio)) {
                        item.url.contains("ashdi.vip")
                    } else {
                        true
                    }
                }

                // Deduplicate & sort: voiceover first, studio name, then episode number
                val sorted = filteredList.sortedWith(
                    compareByDescending<RawVid> { it.isVoice }
                        .thenBy { it.studio }
                        .thenBy { it.epNum }
                )

                for ((idx, v) in sorted.withIndex()) {
                    val number = if (v.epNum > 0) v.epNum.toString() else (idx + 1).toString()
                    val titleText = buildString {
                        append(v.title)
                        if (v.studio.isNotEmpty()) append(" — ${v.studio}")
                        append(if (v.isVoice) " (Озвучення)" else " (Субтитри)")
                    }

                    episodes.add(
                        Episode(
                            number = number,
                            title = titleText,
                            streamUrls = mapOf("ashdi" to v.url),
                            meta = mapOf("vod_url" to v.url, "page_url" to animeUrl)
                        )
                    )
                }
            }

            if (episodes.isEmpty()) {
                // Fallback for single movies / pages with direct iframes
                val doc = Jsoup.parse(html)
                val iframe = doc.selectFirst("iframe[src*=/vod/], iframe[src*=/serial/], iframe[src*=ashdi]")
                var src = iframe?.attr("src") ?: ""
                if (src.startsWith("//")) src = "https:$src"

                episodes.add(
                    Episode(
                        number = "1",
                        title = "Серія 1",
                        streamUrls = if (src.isNotEmpty()) mapOf("ashdi" to src) else emptyMap(),
                        meta = mapOf("vod_url" to src, "page_url" to animeUrl)
                    )
                )
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }

        if (episodes.isEmpty()) {
            episodes.add(Episode(number = "1", title = "Серія 1", meta = mapOf("page_url" to animeUrl)))
        }
        episodes
    }

    override suspend fun getStream(anime: Anime, episode: Episode): Stream = withContext(Dispatchers.IO) {
        var vodUrl = episode.meta["vod_url"] ?: episode.streamUrls["ashdi"] ?: ""
        
        // Automatic fallback: if vodUrl was missing (e.g. from fallback episode), fetch real episodes or scrape page iframe
        if (vodUrl.isEmpty()) {
            try {
                val eps = getEpisodes(anime)
                val matched = eps.firstOrNull { it.number == episode.number } ?: eps.firstOrNull()
                if (matched != null) {
                    vodUrl = matched.meta["vod_url"] ?: matched.streamUrls["ashdi"] ?: ""
                }
            } catch (e: Exception) {}
        }

        val animeUrl = episode.meta["page_url"] ?: anime.meta["url"] ?: "$baseUrl/anime/${anime.id}.html"
        if (vodUrl.isEmpty() && animeUrl.isNotEmpty()) {
            try {
                val req = Request.Builder()
                    .url(animeUrl)
                    .header("User-Agent", NetworkClient.USER_AGENT)
                    .header("Referer", "$baseUrl/")
                    .build()
                val html = NetworkClient.client.newCall(req).execute().body?.string() ?: ""
                val doc = Jsoup.parse(html)
                val iframe = doc.selectFirst("iframe[src*=/vod/], iframe[src*=/serial/], iframe[src*=ashdi]")
                var src = iframe?.attr("src") ?: ""
                if (src.startsWith("//")) src = "https:$src"
                if (src.isNotEmpty()) vodUrl = src
            } catch (e: Exception) {}
        }

        if (vodUrl.isEmpty()) return@withContext Stream()

        val ashdiHeaders = mapOf(
            "Referer" to "https://ashdi.vip/",
            "User-Agent" to NetworkClient.USER_AGENT
        )

        try {
            val req = Request.Builder()
                .url(vodUrl)
                .header("Referer", "$baseUrl/")
                .header("User-Agent", NetworkClient.USER_AGENT)
                .build()

            val resp = NetworkClient.client.newCall(req).execute()
            val body = resp.body?.string() ?: ""

            val masterMatch = Regex("""file:\s*['"](https?://[^'"]+\.m3u8[^'"]*)['"]""").find(body)
                ?: Regex("""(https?://[^\s"'<>]+\.m3u8[^\s"'<>]*)""").find(body)

            val masterM3u8 = masterMatch?.groupValues?.get(1) ?: ""
            if (masterM3u8.isNotEmpty()) {
                val qualities = mutableListOf<Quality>()

                // Fetch master playlist to parse individual qualities (1080p, 720p, 480p)
                try {
                    val mReq = Request.Builder()
                        .url(masterM3u8)
                        .header("Referer", "https://ashdi.vip/")
                        .header("User-Agent", NetworkClient.USER_AGENT)
                        .build()
                    val mResp = NetworkClient.client.newCall(mReq).execute()
                    val mBody = mResp.body?.string() ?: ""

                    var currentRes = ""
                    for (line in mBody.lines()) {
                        val trimmed = line.trim()
                        if (trimmed.startsWith("#EXT-X-STREAM-INF:")) {
                            val resMatch = Regex("""RESOLUTION=\d+x(\d+)""").find(trimmed)
                            if (resMatch != null) {
                                currentRes = "${resMatch.groupValues[1]}p"
                            }
                        } else if (trimmed.isNotEmpty() && !trimmed.startsWith("#")) {
                            var streamUrl = trimmed
                            if (!streamUrl.startsWith("http")) {
                                val lastSlash = masterM3u8.lastIndexOf('/')
                                if (lastSlash != -1) {
                                    streamUrl = masterM3u8.substring(0, lastSlash + 1) + streamUrl
                                }
                            }
                            qualities.add(
                                Quality(
                                    label = if (currentRes.isNotEmpty()) currentRes else "HD",
                                    url = streamUrl,
                                    headers = ashdiHeaders
                                )
                            )
                            currentRes = ""
                        }
                    }
                } catch (e: Exception) {
                    e.printStackTrace()
                }

                // Add master URL at top
                qualities.add(
                    0,
                    Quality(
                        label = "Auto",
                        url = masterM3u8,
                        headers = ashdiHeaders
                    )
                )

                return@withContext Stream(
                    url = masterM3u8,
                    quality = "Auto",
                    headers = ashdiHeaders,
                    qualities = qualities
                )
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }

        Stream(
            url = vodUrl,
            quality = "fallback",
            headers = mapOf("Referer" to "$baseUrl/", "User-Agent" to NetworkClient.USER_AGENT),
            qualities = listOf(Quality("fallback", vodUrl, mapOf("Referer" to "$baseUrl/")))
        )
    }

    private fun mapGenreToSlug(genre: String): String {
        val lower = genre.lowercase().trim()
        return when {
            "комед" in lower -> "comedy"
            "драм" in lower -> "drama"
            "романт" in lower -> "romance"
            "фентез" in lower -> "fantasy"
            "ісек" in lower -> "isekai"
            "екшн" in lower || "боєвик" in lower -> "action"
            "пригод" in lower -> "adventure"
            else -> "shounen"
        }
    }
}

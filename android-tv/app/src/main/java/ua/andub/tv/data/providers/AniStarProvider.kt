package ua.andub.tv.data.providers

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.Request
import ua.andub.tv.data.models.Anime
import ua.andub.tv.data.models.Episode
import ua.andub.tv.data.models.Quality
import ua.andub.tv.data.models.Stream
import java.net.URLEncoder
import java.nio.charset.Charset
import java.util.regex.Pattern

class AniStarProvider : BaseProvider() {
    override val name: String = "AniStar"
    override val displayName: String = "AniStar"

    private val baseUrl = "https://v30.astar.bz"

    companion object {
        @Volatile
        private var challengeCookie: String = "l7_browser=fxgfCS8T5Ez-wqEvekGhiKULOD4PPUH7fDPe30yd-ww"

        private fun updateCookie(html: String) {
            val re = Pattern.compile("l7_browser=([a-zA-Z0-9_-]+)")
            val m = re.matcher(html)
            if (m.find()) {
                challengeCookie = "l7_browser=" + m.group(1)
            }
        }
    }

    private fun fetchPage(url: String, referer: String = "$baseUrl/", isCp1251: Boolean = true): String {
        val reqBuilder = Request.Builder()
            .url(url)
            .header("User-Agent", NetworkClient.USER_AGENT)
            .header("Referer", referer)

        if (challengeCookie.isNotBlank()) {
            reqBuilder.header("Cookie", challengeCookie)
        }

        var resp = NetworkClient.client.newCall(reqBuilder.build()).execute()
        val bytes = resp.body?.bytes() ?: return ""
        val charset = if (isCp1251) Charset.forName("windows-1251") else Charsets.UTF_8
        var bodyStr = String(bytes, charset)

        if (bodyStr.contains("Проверяем ваш браузер") || bodyStr.contains("document.cookie")) {
            updateCookie(bodyStr)
            val retryReq = Request.Builder()
                .url(url)
                .header("User-Agent", NetworkClient.USER_AGENT)
                .header("Referer", referer)
                .header("Cookie", challengeCookie)
                .build()
            resp = NetworkClient.client.newCall(retryReq).execute()
            val retryBytes = resp.body?.bytes() ?: return ""
            bodyStr = String(retryBytes, charset)
        }

        return bodyStr
    }

    override suspend fun search(
        query: String,
        limit: Int,
        genre: String,
        page: Int
    ): List<Anime> = withContext(Dispatchers.IO) {
        val results = mutableListOf<Anime>()
        try {
            val isHentai = genre.contains("хентай", ignoreCase = true) || genre.contains("hentai", ignoreCase = true)
            val url = when {
                isHentai -> {
                    if (query.isBlank()) {
                        if (page > 1) "$baseUrl/hentai/page/$page/" else "$baseUrl/hentai/"
                    } else {
                        val enc = URLEncoder.encode(query, "windows-1251")
                        "$baseUrl/index.php?do=search&subaction=search&story=$enc" + (if (page > 1) "&search_start=$page" else "")
                    }
                }
                query.isNotBlank() -> {
                    val enc = URLEncoder.encode(query, "windows-1251")
                    "$baseUrl/index.php?do=search&subaction=search&story=$enc" + (if (page > 1) "&search_start=$page" else "")
                }
                genre.isNotBlank() && !genre.equals("Все жанры", ignoreCase = true) -> {
                    val enc = URLEncoder.encode(genre, "windows-1251")
                    if (page > 1) "$baseUrl/filter/janrs/$enc/page/$page/" else "$baseUrl/filter/janrs/$enc/"
                }
                else -> {
                    if (page > 1) "$baseUrl/anime/page/$page/" else "$baseUrl/anime/"
                }
            }

            val html = fetchPage(url, isCp1251 = true)
            val itemPattern = Pattern.compile("<div\\s+class=\"news\"[\\s\\S]*?(?=<div\\s+class=\"news\"|<div\\s+class=\"pagenav\"|$)")
            val itemMatcher = itemPattern.matcher(html)

            while (itemMatcher.find() && results.size < limit) {
                val block = itemMatcher.group()

                val titlePattern = Pattern.compile("<div\\s+class=\"title_left\"><a\\s+href=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</a>")
                val titleMatcher = titlePattern.matcher(block)
                if (!titleMatcher.find()) continue

                val itemUrl = titleMatcher.group(1) ?: ""
                val rawTitle = cleanTags(titleMatcher.group(2) ?: "")

                if (itemUrl.contains("igra")) continue
                val idPattern = Pattern.compile("/(\\d+)-")
                val idMatcher = idPattern.matcher(itemUrl)
                if (!idMatcher.find()) continue
                val newsId = idMatcher.group(1) ?: ""

                var titleRu = rawTitle
                var titleEn = ""
                val slashPos = rawTitle.indexOf(" / ")
                if (slashPos != -1) {
                    titleRu = rawTitle.substring(0, slashPos).trim()
                    titleEn = rawTitle.substring(slashPos + 3).trim()
                }

                // Poster
                var posterUrl = ""
                val imgPattern = Pattern.compile("<img[^>]+(?:class=\"main-img\"[^>]+src=\"([^\"]+)\"|src=\"([^\"]+)\"[^>]+class=\"main-img\"|itemprop=\"image\"[^>]+src=\"([^\"]+)\"|src=\"([^\"]+)\"[^>]+itemprop=\"image\")")
                val imgMatcher = imgPattern.matcher(block)
                if (imgMatcher.find()) {
                    for (i in 1..imgMatcher.groupCount()) {
                        val g = imgMatcher.group(i)
                        if (!g.isNullOrBlank()) {
                            posterUrl = if (g.startsWith("http")) g else "$baseUrl$g"
                            break
                        }
                    }
                }

                // Year
                var year = 0
                val yearPattern = Pattern.compile("Год выпуска:[\\s\\S]*?(\\d{4})")
                val yearMatcher = yearPattern.matcher(block)
                if (yearMatcher.find()) {
                    year = yearMatcher.group(1)?.toIntOrNull() ?: 0
                }

                // Rating
                var rating = ""
                val ratPattern = Pattern.compile("itemprop=\"ratingValue\">([^<]+)<")
                val ratMatcher = ratPattern.matcher(block)
                if (ratMatcher.find()) {
                    rating = ratMatcher.group(1) ?: ""
                }

                // Status
                var status = ""
                val epPattern = Pattern.compile("Серии:[\\s\\S]*?</b>([^<]+)<")
                val epMatcher = epPattern.matcher(block)
                if (epMatcher.find()) {
                    status = cleanTags(epMatcher.group(1) ?: "")
                }

                // Genres
                val genresSet = mutableSetOf<String>()
                val tagsPattern = Pattern.compile("<p class=\"tags\">([\\s\\S]*?)</p>")
                val tagsMatcher = tagsPattern.matcher(block)
                if (tagsMatcher.find()) {
                    val aPattern = Pattern.compile(">([^<]+)</a>")
                    val aMatcher = aPattern.matcher(tagsMatcher.group(1) ?: "")
                    while (aMatcher.find()) {
                        val g = cleanTags(aMatcher.group(1) ?: "")
                        if (g.isNotBlank() && g != "Аниме") genresSet.add(g)
                    }
                }

                val janrsPattern = Pattern.compile("<b>Жанр:[\\s\\S]*?</b>([\\s\\S]*?)</li>")
                val janrsMatcher = janrsPattern.matcher(block)
                if (janrsMatcher.find()) {
                    val aPattern = Pattern.compile(">([^<]+)</a>")
                    val aMatcher = aPattern.matcher(janrsMatcher.group(1) ?: "")
                    while (aMatcher.find()) {
                        val g = cleanTags(aMatcher.group(1) ?: "")
                        if (g.isNotBlank()) genresSet.add(g)
                    }
                }

                // Description
                var description = ""
                val descPattern = Pattern.compile("<div\\s+class=\"descripts\">([\\s\\S]*?)</div>")
                val descMatcher = descPattern.matcher(block)
                if (descMatcher.find()) {
                    description = cleanTags(descMatcher.group(1) ?: "")
                }

                results.add(
                    Anime(
                        id = newsId,
                        titleRu = titleRu,
                        titleEn = titleEn,
                        description = description,
                        year = year,
                        genres = genresSet.toList(),
                        status = status,
                        provider = name,
                        posterUrl = posterUrl,
                        rating = rating,
                        meta = mapOf("url" to itemUrl, "news_id" to newsId)
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
            var newsId = anime.meta["news_id"] ?: anime.id
            if (newsId.contains("/") || newsId.contains("-")) {
                val m = Pattern.compile("/(\\d+)-").matcher(newsId)
                if (m.find()) newsId = m.group(1) ?: newsId
            }

            val referer = anime.meta["url"] ?: "$baseUrl/"
            val playerUrl = "$baseUrl/test/player2/videoas_p2p_new.php?id=$newsId"
            val pHtml = fetchPage(playerUrl, referer = referer, isCp1251 = false)

            val epPattern = Pattern.compile("\\{\\s*title:\"([^\"]+)\"([\\s\\S]*?files_mp4:\\s*\\[[\\s\\S]*?\\])")
            val epMatcher = epPattern.matcher(pHtml)

            var idx = 1
            while (epMatcher.find()) {
                val epTitle = epMatcher.group(1) ?: "Серія $idx"
                val epBody = epMatcher.group(2) ?: ""

                var epNum = idx.toString()
                val numMatcher = Pattern.compile("(\\d+)").matcher(epTitle)
                if (numMatcher.find()) {
                    epNum = numMatcher.group(1) ?: idx.toString()
                }

                val streamUrls = mutableMapOf<String, String>()

                // Direct MP4 streams
                val mp4BlockMatcher = Pattern.compile("files_mp4:\\s*\\[([\\s\\S]*?)\\]").matcher(epBody)
                if (mp4BlockMatcher.find()) {
                    val fileMatcher = Pattern.compile("title:\"([^\"]+)\",\\s*file:\"([^\"]+)\"").matcher(mp4BlockMatcher.group(1) ?: "")
                    while (fileMatcher.find()) {
                        val q = fileMatcher.group(1) ?: ""
                        val f = fileMatcher.group(2) ?: ""
                        val label = when (q) {
                            "720" -> "720p"
                            "360" -> "360p"
                            "1080" -> "1080p"
                            "480" -> "480p"
                            else -> if (q.endsWith("p")) q else "${q}p"
                        }
                        if (f.isNotBlank()) streamUrls[label] = f
                    }
                }

                // Fallback HLS
                val hlsBlockMatcher = Pattern.compile("files:\\s*\\[([\\s\\S]*?)\\]").matcher(epBody)
                if (hlsBlockMatcher.find()) {
                    val fileMatcher = Pattern.compile("title:\"([^\"]+)\",\\s*file:\"([^\"]+)\"").matcher(hlsBlockMatcher.group(1) ?: "")
                    while (fileMatcher.find()) {
                        val q = fileMatcher.group(1) ?: ""
                        val f = fileMatcher.group(2) ?: ""
                        val label = if (q.endsWith("p")) q else "${q}p"
                        if (f.isNotBlank() && !streamUrls.containsKey(label)) {
                            streamUrls[label] = f
                        }
                    }
                }

                episodes.add(
                    Episode(
                        number = epNum,
                        title = epTitle,
                        streamUrls = streamUrls,
                        meta = mapOf("news_id" to newsId, "referer" to "$baseUrl/")
                    )
                )
                idx++
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        episodes
    }

    override suspend fun getStream(anime: Anime, episode: Episode): Stream = withContext(Dispatchers.IO) {
        var urls = episode.streamUrls
        if (urls.isEmpty()) {
            val allEps = getEpisodes(anime)
            val found = allEps.firstOrNull { it.number == episode.number } ?: allEps.firstOrNull()
            if (found != null) urls = found.streamUrls
        }

        val qualities = mutableListOf<Quality>()
        val order = listOf("1080p", "720p", "480p", "360p")
        val headers = mapOf("Referer" to "$baseUrl/", "User-Agent" to NetworkClient.USER_AGENT)

        for (target in order) {
            val u = urls[target]
            if (!u.isNullOrBlank()) {
                qualities.add(Quality(label = target, url = u, headers = headers))
            }
        }

        for ((label, u) in urls) {
            if (u.isNotBlank() && !order.contains(label)) {
                qualities.add(Quality(label = label, url = u, headers = headers))
            }
        }

        val primaryUrl = qualities.firstOrNull()?.url ?: ""
        Stream(
            url = primaryUrl,
            quality = qualities.firstOrNull()?.label ?: "720p",
            headers = headers,
            qualities = qualities
        )
    }

    private fun cleanTags(html: String): String {
        return html
            .replace(Regex("<p class=\"reason\">[\\s\\S]*?</p>"), "")
            .replace(Regex("<br\\s*/?>"), "\n")
            .replace(Regex("<[^>]+>"), "")
            .replace("&quot;", "\"")
            .replace("&amp;", "&")
            .replace("&lt;", "<")
            .replace("&gt;", ">")
            .replace("&#39;", "'")
            .replace("&nbsp;", " ")
            .trim()
    }
}

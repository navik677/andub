package ua.andub.tv.data.providers

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.Request
import org.json.JSONObject
import org.jsoup.Jsoup
import ua.andub.tv.data.models.Anime
import ua.andub.tv.data.models.Episode
import ua.andub.tv.data.models.Stream
import java.net.URLEncoder
import java.util.regex.Pattern

class AniBazaProvider : BaseProvider() {
    override val name: String = "AniBaza"
    override val displayName: String = "AniBaza"

    private val baseUrl = "https://anibaza.com"

    private val genreMap = mapOf(
        "екшн" to "экшен",
        "фентезі" to "фэнтези",
        "комедія" to "комедия",
        "пригоди" to "приключения",
        "романтика" to "романтика",
        "драма" to "драма",
        "детектив" to "детектив",
        "жахи" to "ужасы",
        "містика" to "мистика",
        "спорт" to "спорт",
        "фантастика" to "фантастика",
        "повсякденність" to "повседневность",
        "трилер" to "триллер",
        "школа" to "школа",
        "сьонен" to "сёнен"
    )

    override suspend fun search(
        query: String,
        limit: Int,
        genre: String,
        page: Int
    ): List<Anime> = withContext(Dispatchers.IO) {
        val results = mutableListOf<Anime>()
        val skipCount = if (page > 1) (page - 1) * limit else 0

        try {
            if (query.isBlank() && genre.isBlank()) {
                // Scrape home page cards if no query, matching PC behavior
                val req = Request.Builder()
                    .url(baseUrl)
                    .header("User-Agent", NetworkClient.USER_AGENT)
                    .header("Referer", "$baseUrl/")
                    .build()
                val resp = NetworkClient.client.newCall(req).execute()
                val html = resp.body?.string() ?: ""
                val doc = Jsoup.parse(html)

                val seenIds = mutableSetOf<String>()

                // 1. Scrape standard & cut cards
                val cardElements = doc.select(".release__card, .carousel__item-wrap, a[href*=/release/]")
                for (card in cardElements) {
                    val linkHref = if (card.tagName() == "a") card.attr("href") else card.selectFirst("a[href*=/release/]")?.attr("href") ?: ""
                    if (!linkHref.contains("/release/")) continue

                    val slug = linkHref.trim('/').substringAfterLast('/')
                    if (slug.isEmpty() || seenIds.contains(slug)) continue

                    var title = card.selectFirst(".cut-card__title, .release-title, h2, h3")?.text()?.trim() ?: ""
                    val imgEl = card.selectFirst("img")
                    if (title.isEmpty()) {
                        title = imgEl?.attr("alt")?.trim() ?: ""
                    }
                    if (title.isEmpty()) {
                        title = slug
                    }

                    var poster = imgEl?.attr("src")?.ifEmpty { null }
                        ?: imgEl?.attr("data-src")?.ifEmpty { null }
                        ?: imgEl?.attr("data-original")?.ifEmpty { null }
                        ?: ""

                    if (poster.isNotEmpty() && !poster.startsWith("http")) {
                        poster = if (poster.startsWith("/")) "$baseUrl$poster" else "$baseUrl/$poster"
                    }

                    seenIds.add(slug)
                    results.add(
                        Anime(
                            id = slug,
                            titleRu = title,
                            provider = name,
                            posterUrl = poster,
                            meta = mapOf("slug" to slug, "url" to "$baseUrl/release/$slug/")
                        )
                    )
                }

                if (results.isNotEmpty()) {
                    val paginated = results.drop(skipCount).take(limit)
                    return@withContext paginated
                }
            }

            // Keyword / Genre Search
            var searchQuery = query.trim()
            if (searchQuery.isEmpty() && genre.isNotEmpty()) {
                val gKey = genre.lowercase().trim()
                searchQuery = genreMap[gKey] ?: genre
            }
            if (searchQuery.isEmpty()) {
                searchQuery = "аниме"
            }

            val url = "$baseUrl/search/?query=${URLEncoder.encode(searchQuery, "UTF-8")}"

            val req = Request.Builder()
                .url(url)
                .header("User-Agent", NetworkClient.USER_AGENT)
                .header("X-Requested-With", "XMLHttpRequest")
                .header("Referer", "$baseUrl/")
                .build()

            val resp = NetworkClient.client.newCall(req).execute()
            val body = resp.body?.string() ?: ""

            val root = JSONObject(body)
            val resultsArr = root.optJSONArray("results")

            if (resultsArr != null && resultsArr.length() > 0) {
                for (i in skipCount until resultsArr.length()) {
                    val item = resultsArr.getJSONObject(i)
                    val slug = item.optString("slug")
                    val titleRu = item.optString("rus_title")
                    val titleEn = item.optString("original_title")

                    var poster = item.optString("poster_url")
                    if (poster.isNotEmpty() && !poster.startsWith("http")) {
                        poster = if (poster.startsWith("/")) "$baseUrl$poster" else "$baseUrl/$poster"
                    }

                    results.add(
                        Anime(
                            id = slug,
                            titleRu = titleRu.ifEmpty { titleEn },
                            titleEn = titleEn,
                            provider = name,
                            posterUrl = poster,
                            meta = mapOf("slug" to slug, "url" to "$baseUrl/release/$slug/")
                        )
                    )
                    if (results.size >= limit) break
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        results
    }

    override suspend fun getEpisodes(anime: Anime): List<Episode> = withContext(Dispatchers.IO) {
        val episodes = mutableListOf<Episode>()
        val slug = anime.meta["slug"] ?: anime.id
        val pageUrl = anime.meta["url"] ?: "$baseUrl/release/$slug/"

        try {
            val req = Request.Builder()
                .url(pageUrl)
                .header("User-Agent", NetworkClient.USER_AGENT)
                .header("Referer", "$baseUrl/")
                .build()

            val resp = NetworkClient.client.newCall(req).execute()
            val html = resp.body?.string() ?: ""
            val doc = Jsoup.parse(html)

            // Look for Kodik link in input, iframe or html text
            var kodikUrl = doc.selectFirst("input#kodik-link")?.attr("value") ?: ""
            if (kodikUrl.isEmpty()) {
                kodikUrl = doc.selectFirst("input[name=kodik-embed]")?.attr("value") ?: ""
            }
            if (kodikUrl.isEmpty()) {
                val iframe = doc.selectFirst("iframe[src*=kodik]")
                kodikUrl = iframe?.attr("src") ?: ""
            }
            if (kodikUrl.isEmpty()) {
                val kodikPattern = Pattern.compile("(?:https?:)?//(kodik(?:player)?\\.(?:com|info|biz)/(?:serial|video)/[a-zA-Z0-9_/]+)")
                val match = kodikPattern.matcher(html)
                if (match.find()) {
                    kodikUrl = "https://" + match.group(1)
                }
            }

            if (kodikUrl.startsWith("//")) {
                kodikUrl = "https:$kodikUrl"
            }

            if (kodikUrl.isNotEmpty()) {
                val epList = KodikResolver.getEpisodes(kodikUrl, referer = "$baseUrl/")
                if (epList.isNotEmpty()) {
                    for (epNum in epList) {
                        episodes.add(
                            Episode(
                                number = epNum.toString(),
                                title = "Серія $epNum",
                                meta = mapOf("kodik_url" to kodikUrl, "episode_num" to epNum.toString())
                            )
                        )
                    }
                } else {
                    episodes.add(
                        Episode(
                            number = "1",
                            title = "Серія 1",
                            meta = mapOf("kodik_url" to kodikUrl, "episode_num" to "1")
                        )
                    )
                }
            } else {
                episodes.add(
                    Episode(
                        number = "1",
                        title = "Серія 1",
                        meta = mapOf("page_url" to pageUrl)
                    )
                )
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }

        if (episodes.isEmpty()) {
            episodes.add(Episode(number = "1", title = "Серія 1", meta = mapOf("page_url" to pageUrl)))
        }
        episodes
    }

    override suspend fun getStream(anime: Anime, episode: Episode): Stream = withContext(Dispatchers.IO) {
        var kodikUrl = episode.meta["kodik_url"] ?: ""
        val epNum = episode.meta["episode_num"]?.toIntOrNull() ?: episode.number.toIntOrNull() ?: 1

        if (kodikUrl.isEmpty()) {
            val eps = getEpisodes(anime)
            val matched = eps.firstOrNull { it.number == episode.number } ?: eps.firstOrNull()
            kodikUrl = matched?.meta?.get("kodik_url") ?: ""
        }

        if (kodikUrl.isNotEmpty()) {
            val stream = KodikResolver.resolve(kodikUrl, epNum, referer = "$baseUrl/")
            if (stream.qualities.isNotEmpty()) {
                return@withContext stream
            }
        }
        Stream()
    }
}

package ua.andub.tv.data.providers

import android.util.Base64
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.FormBody
import okhttp3.Request
import org.json.JSONObject
import org.jsoup.Jsoup
import ua.andub.tv.data.models.Quality
import ua.andub.tv.data.models.Stream
import java.util.regex.Pattern

object KodikResolver {

    private fun rot18(input: String): String {
        val sb = StringBuilder(input.length)
        for (c in input) {
            when (c) {
                in 'a'..'z' -> sb.append(((c - 'a' + 18) % 26 + 'a'.code).toChar())
                in 'A'..'Z' -> sb.append(((c - 'A' + 18) % 26 + 'A'.code).toChar())
                else -> sb.append(c)
            }
        }
        return sb.toString()
    }

    private fun decodeKodikLink(raw: String): String {
        return try {
            var rot = rot18(raw)
            val rem = rot.length % 4
            if (rem > 0) {
                rot += "=".repeat(4 - rem)
            }
            val decoded = String(Base64.decode(rot, Base64.DEFAULT))
            if (decoded.startsWith("//")) "https:$decoded" else decoded
        } catch (e: Exception) {
            raw
        }
    }

    suspend fun getEpisodes(rawKodikUrl: String, referer: String = "https://kodikplayer.com/"): List<Int> = withContext(Dispatchers.IO) {
        val episodes = mutableListOf<Int>()
        var url = rawKodikUrl
        if (url.startsWith("//")) url = "https:$url"

        try {
            val req = Request.Builder()
                .url(url)
                .header("User-Agent", NetworkClient.USER_AGENT)
                .header("Referer", referer)
                .build()

            val resp = NetworkClient.client.newCall(req).execute()
            val html = resp.body?.string() ?: ""

            // 1. Try Jsoup parsing of option tags
            val doc = Jsoup.parse(html)
            val options = doc.select("option[data-id][data-hash]")
            for (opt in options) {
                val valueStr = opt.attr("value")
                val ep = valueStr.toIntOrNull()
                if (ep != null && !episodes.contains(ep)) {
                    episodes.add(ep)
                }
            }

            // 2. Fallback regex
            if (episodes.isEmpty()) {
                val optPattern = Pattern.compile("<option\\s+[^>]*value=[\"'](\\d+)[\"'][^>]*data-id=[\"'](\\d+)[\"'][^>]*data-hash=[\"']([a-zA-Z0-9]+)[\"']")
                val matcher = optPattern.matcher(html)
                while (matcher.find()) {
                    val ep = matcher.group(1)?.toIntOrNull()
                    if (ep != null && !episodes.contains(ep)) {
                        episodes.add(ep)
                    }
                }
            }

            // 3. Fallback to episode count
            if (episodes.isEmpty()) {
                val countMatch = Regex("""data-episode-count="(\d+)"""").find(html)
                val count = countMatch?.groupValues?.get(1)?.toIntOrNull() ?: 0
                for (i in 1..count) {
                    episodes.add(i)
                }
            }

            // 4. If still empty but it's a valid video page, at least episode 1 exists
            if (episodes.isEmpty() && (html.contains("serialId") || html.contains("videoInfo") || html.contains("vInfo"))) {
                episodes.add(1)
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        episodes
    }

    suspend fun resolve(rawKodikUrl: String, episodeNum: Int = 1, referer: String = "https://kodikplayer.com/"): Stream = withContext(Dispatchers.IO) {
        var url = rawKodikUrl
        if (url.startsWith("//")) url = "https:$url"

        try {
            val req = Request.Builder()
                .url(url)
                .header("User-Agent", NetworkClient.USER_AGENT)
                .header("Referer", referer)
                .build()

            val resp = NetworkClient.client.newCall(req).execute()
            val html = resp.body?.string() ?: ""

            var domain = ""
            var dSign = ""
            var pd = "kodikplayer.com"
            var pdSign = ""
            var ref = ""
            var refSign = ""

            Regex("""var\s+domain\s*=\s*['"]([^'"]+)['"]""").find(html)?.let { domain = it.groupValues[1] }
            Regex("""var\s+d_sign\s*=\s*['"]([^'"]+)['"]""").find(html)?.let { dSign = it.groupValues[1] }
            Regex("""var\s+pd\s*=\s*['"]([^'"]+)['"]""").find(html)?.let { pd = it.groupValues[1] }
            Regex("""var\s+pd_sign\s*=\s*['"]([^'"]+)['"]""").find(html)?.let { pdSign = it.groupValues[1] }
            Regex("""var\s+ref\s*=\s*['"]([^'"]*)['"]""").find(html)?.let { ref = it.groupValues[1] }
            Regex("""var\s+ref_sign\s*=\s*['"]([^'"]*)['"]""").find(html)?.let { refSign = it.groupValues[1] }

            // If urlParams exists in html
            val urlParamsMatch = Regex("""urlParams\s*=\s*'([^']+)'""").find(html)
            if (urlParamsMatch != null) {
                try {
                    val pObj = JSONObject(urlParamsMatch.groupValues[1])
                    if (domain.isEmpty()) domain = pObj.optString("d")
                    if (dSign.isEmpty()) dSign = pObj.optString("d_sign")
                    if (pd.isEmpty() || pd == "kodikplayer.com") pd = pObj.optString("pd", pd)
                    if (pdSign.isEmpty()) pdSign = pObj.optString("pd_sign")
                    if (ref.isEmpty()) ref = pObj.optString("ref")
                    if (refSign.isEmpty()) refSign = pObj.optString("ref_sign")
                } catch (e: Exception) {}
            }

            // Decode URL-encoded ref if needed
            if (ref.contains("%")) {
                try {
                    ref = java.net.URLDecoder.decode(ref, "UTF-8")
                } catch (e: Exception) {}
            }

            var vidId = ""
            var vidHash = ""
            var vidType = "seria"

            // Jsoup selection for specific episode
            val doc = Jsoup.parse(html)
            val specificOpt = doc.selectFirst("option[value=$episodeNum][data-id][data-hash]")
            if (specificOpt != null) {
                vidId = specificOpt.attr("data-id")
                vidHash = specificOpt.attr("data-hash")
            } else {
                val anyOpt = doc.selectFirst("option[data-id][data-hash]")
                if (anyOpt != null) {
                    vidId = anyOpt.attr("data-id")
                    vidHash = anyOpt.attr("data-hash")
                }
            }

            // Fallback to regex if Jsoup missed it
            if (vidId.isEmpty() || vidHash.isEmpty()) {
                val targetOpt = Pattern.compile("<option\\s+[^>]*value=[\"']$episodeNum[\"'][^>]*data-id=[\"'](\\d+)[\"'][^>]*data-hash=[\"']([a-zA-Z0-9]+)[\"']")
                val optMatcher = targetOpt.matcher(html)
                if (optMatcher.find()) {
                    vidId = optMatcher.group(1) ?: ""
                    vidHash = optMatcher.group(2) ?: ""
                }
            }

            // Fallback to vInfo / serialId
            if (vidId.isEmpty() || vidHash.isEmpty()) {
                Regex("""vInfo\.id\s*=\s*['"]([^'"]+)['"]""").find(html)?.let { vidId = it.groupValues[1] }
                Regex("""vInfo\.hash\s*=\s*['"]([^'"]+)['"]""").find(html)?.let { vidHash = it.groupValues[1] }
                Regex("""vInfo\.type\s*=\s*['"]([^'"]+)['"]""").find(html)?.let { vidType = it.groupValues[1] }
            }

            if (vidId.isEmpty() || vidHash.isEmpty()) {
                Regex("""serialId\s*=\s*Number\((\d+)\)""").find(html)?.let { vidId = it.groupValues[1] }
                Regex("""serialHash\s*=\s*['"]([^'"]+)['"]""").find(html)?.let { vidHash = it.groupValues[1] }
            }

            if (vidId.isNotEmpty() && vidHash.isNotEmpty()) {
                val ftorUrl = "https://$pd/ftor"
                val formBuilder = FormBody.Builder()
                    .add("d", domain)
                    .add("d_sign", dSign)
                    .add("pd", pd)
                    .add("pd_sign", pdSign)
                    .add("ref", ref)
                    .add("ref_sign", refSign)
                    .add("bad_user", "false")
                    .add("cdn_is_working", "true")
                    .add("type", vidType)
                    .add("hash", vidHash)
                    .add("id", vidId)

                val postReq = Request.Builder()
                    .url(ftorUrl)
                    .post(formBuilder.build())
                    .header("User-Agent", NetworkClient.USER_AGENT)
                    .header("Referer", url)
                    .header("Origin", "https://$pd")
                    .header("X-Requested-With", "XMLHttpRequest")
                    .build()

                val postResp = NetworkClient.client.newCall(postReq).execute()
                val jsonStr = postResp.body?.string() ?: ""
                val root = JSONObject(jsonStr)
                val links = root.optJSONObject("links")

                if (links != null) {
                    val qualities = mutableListOf<Quality>()
                    val streamHeaders = mapOf(
                        "Referer" to "https://$pd/",
                        "User-Agent" to NetworkClient.USER_AGENT
                    )

                    val qualityOrder = listOf("1080", "720", "480", "360")
                    for (qKey in qualityOrder) {
                        val qArr = links.optJSONArray(qKey)
                        if (qArr != null && qArr.length() > 0) {
                            val rawSrc = qArr.getJSONObject(0).optString("src")
                            if (rawSrc.isNotEmpty()) {
                                var directUrl = decodeKodikLink(rawSrc)
                                if (directUrl.startsWith("//")) directUrl = "https:$directUrl"
                                if (directUrl.contains(":hls:manifest.m3u8")) {
                                    directUrl = directUrl.substringBefore(":hls:manifest.m3u8")
                                }
                                
                                // Resolve 302 redirect (cloud.solodcdn.com -> green.cloud.solodcdn.com)
                                val finalUrl = resolveFinalCdnUrl(directUrl, "https://$pd/")

                                qualities.add(
                                    Quality(
                                        label = "${qKey}p",
                                        url = finalUrl,
                                        headers = streamHeaders
                                    )
                                )
                            }
                        }
                    }

                    if (qualities.isNotEmpty()) {
                        return@withContext Stream(
                            url = qualities.first().url,
                            quality = qualities.first().label,
                            headers = streamHeaders,
                            qualities = qualities
                        )
                    }
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }

        Stream()
    }

    private fun resolveFinalCdnUrl(initialUrl: String, referer: String): String {
        return try {
            val req = Request.Builder()
                .url(initialUrl)
                .header("User-Agent", NetworkClient.USER_AGENT)
                .header("Referer", referer)
                .build()
            NetworkClient.client.newCall(req).execute().use { resp ->
                resp.request.url.toString()
            }
        } catch (e: Exception) {
            initialUrl
        }
    }

    suspend fun resolveStreamUrl(kodikUrl: String): String = withContext(Dispatchers.IO) {
        val stream = resolve(kodikUrl, 1)
        stream.primaryUrl
    }
}

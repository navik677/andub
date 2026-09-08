package ua.andub.tv.ui.common

import com.bumptech.glide.load.model.GlideUrl
import com.bumptech.glide.load.model.LazyHeaders

object GlideUtils {

    private const val USER_AGENT =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"

    /**
     * Builds a Glide-compatible URL model with anti-hotlink Referer headers
     * for providers like AniTube, AniDub, and AniBaza that reject direct requests with 403 Forbidden.
     */
    fun buildGlideUrl(url: String): Any {
        if (url.isBlank()) return ""
        val referer = when {
            url.contains("anitube", ignoreCase = true) -> "https://anitube.in.ua/"
            url.contains("anidub", ignoreCase = true) -> "https://anidub.vip/"
            url.contains("anibaza", ignoreCase = true) -> "https://anibaza.com/"
            url.contains("animevost", ignoreCase = true) -> "https://animevost.org/"
            else -> null
        }

        return if (referer != null) {
            GlideUrl(
                url,
                LazyHeaders.Builder()
                    .addHeader("User-Agent", USER_AGENT)
                    .addHeader("Referer", referer)
                    .build()
            )
        } else {
            url
        }
    }
}

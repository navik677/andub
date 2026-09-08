package ua.andub.tv.data.models

import java.io.Serializable

data class Subtitle(
    val language: String,
    val url: String
) : Serializable

data class Quality(
    val label: String = "auto",
    val url: String = "",
    val headers: Map<String, String> = emptyMap()
) : Serializable

data class Stream(
    val url: String = "",
    val quality: String = "auto",
    val headers: Map<String, String> = emptyMap(),
    val qualities: List<Quality> = emptyList(),
    val subtitles: List<Subtitle> = emptyList()
) : Serializable {

    val primaryUrl: String
        get() = url.ifEmpty { qualities.firstOrNull { it.url.isNotEmpty() }?.url ?: "" }

    val primaryHeaders: Map<String, String>
        get() = if (headers.isNotEmpty()) headers else qualities.firstOrNull { it.url.isNotEmpty() }?.headers ?: emptyMap()
}

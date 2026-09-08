package ua.andub.tv.data.models

import java.io.Serializable

data class Episode(
    val number: String,
    val title: String = "",
    val streamUrls: Map<String, String> = emptyMap(), // "1080p", "720p", "480p", "Auto"
    val openingSkip: List<Int> = emptyList(),          // [start_sec, end_sec]
    val endingSkip: List<Int> = emptyList(),           // [start_sec, end_sec]
    val meta: Map<String, String> = emptyMap()
) : Serializable {

    fun displayTitle(): String {
        return if (title.isNotBlank() && title != number) {
            "Серія $number: $title"
        } else {
            "Серія $number"
        }
    }
}

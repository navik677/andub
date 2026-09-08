package ua.andub.tv.data.models

import java.io.Serializable

data class Anime(
    val id: String,
    val titleRu: String,
    val titleEn: String = "",
    val description: String = "",
    val year: Int = 0,
    val genres: List<String> = emptyList(),
    val status: String = "",
    val provider: String = "",
    val posterUrl: String = "",
    val rating: String = "",
    val ageRating: String = "",
    val meta: Map<String, String> = emptyMap()
) : Serializable {

    val primaryTitle: String
        get() = if (titleRu.isNotBlank()) titleRu else if (titleEn.isNotBlank()) titleEn else "Без назви"

    fun displayTitle(): String {
        val title = primaryTitle
        return when {
            titleEn.isNotBlank() && titleEn != titleRu && year > 0 -> "$title ($titleEn) [$year]"
            titleEn.isNotBlank() && titleEn != titleRu -> "$title ($titleEn)"
            year > 0 -> "$title [$year]"
            else -> title
        }
    }

    fun genresStr(): String = genres.joinToString(", ")
}

package ua.andub.tv.ui.details

import androidx.leanback.widget.AbstractDetailsDescriptionPresenter
import ua.andub.tv.data.models.Anime

class DetailsDescriptionPresenter : AbstractDetailsDescriptionPresenter() {

    override fun onBindDescription(viewHolder: ViewHolder, item: Any?) {
        val anime = item as? Anime ?: return

        viewHolder.title.text = anime.primaryTitle

        val sub = buildString {
            if (anime.rating.isNotEmpty()) append("${anime.rating}  •  ")
            if (anime.year > 0) append("${anime.year}  •  ")
            if (anime.genres.isNotEmpty()) append(anime.genres.take(3).joinToString(", "))
            else append(anime.provider)
            append("  •  ${anime.provider}")
        }
        viewHolder.subtitle.text = sub

        val cleanDesc = anime.description
            .replace(Regex("<[^>]*>"), "")
            .trim()

        viewHolder.body.text = if (cleanDesc.isNotBlank() && cleanDesc != "null" && cleanDesc != "None") cleanDesc else "Опис аніме тимчасово відсутній"
    }
}

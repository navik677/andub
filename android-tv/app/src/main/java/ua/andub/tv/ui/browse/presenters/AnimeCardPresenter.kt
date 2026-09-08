package ua.andub.tv.ui.browse.presenters

import android.graphics.Color
import android.graphics.Outline
import android.view.View
import android.view.ViewGroup
import android.view.ViewOutlineProvider
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.leanback.widget.ImageCardView
import androidx.leanback.widget.Presenter
import com.bumptech.glide.Glide
import com.bumptech.glide.load.resource.drawable.DrawableTransitionOptions
import ua.andub.tv.R
import ua.andub.tv.data.models.Anime

class AnimeCardPresenter : Presenter() {

    override fun onCreateViewHolder(parent: ViewGroup): ViewHolder {
        val context = parent.context
        val density = context.resources.displayMetrics.density

        val cardView = object : ImageCardView(context) {
            override fun setSelected(selected: Boolean) {
                super.setSelected(selected)
                animate()
                    .scaleX(if (selected) 1.05f else 1.0f)
                    .scaleY(if (selected) 1.05f else 1.0f)
                    .translationZ(if (selected) 12 * density else 0f)
                    .setDuration(160)
                    .start()
            }
        }.apply {
            isFocusable = true
            isFocusableInTouchMode = true
            setBackgroundResource(R.drawable.bg_card_selector)
            setInfoAreaBackgroundColor(Color.TRANSPARENT)

            val width = context.resources.getDimensionPixelSize(R.dimen.card_width)
            val height = context.resources.getDimensionPixelSize(R.dimen.card_height)
            setMainImageDimensions(width, height)

            // Clean 10dp rounded corners on poster
            mainImageView?.outlineProvider = object : ViewOutlineProvider() {
                override fun getOutline(view: View, outline: Outline) {
                    outline.setRoundRect(0, 0, view.width, view.height, 10 * density)
                }
            }
            mainImageView?.clipToOutline = true

            // Clean modern typography without AI-ish contrast
            findViewById<TextView>(androidx.leanback.R.id.title_text)?.apply {
                setTextColor(ContextCompat.getColor(context, R.color.text_primary))
                textSize = 12.5f
                maxLines = 1
            }
            findViewById<TextView>(androidx.leanback.R.id.content_text)?.apply {
                setTextColor(ContextCompat.getColor(context, R.color.text_secondary))
                textSize = 11f
                maxLines = 1
            }
        }
        return ViewHolder(cardView)
    }

    override fun onBindViewHolder(viewHolder: ViewHolder, item: Any?) {
        val anime = item as? Anime ?: return
        val cardView = viewHolder.view as ImageCardView

        cardView.titleText = anime.primaryTitle

        val details = buildString {
            if (anime.rating.isNotEmpty()) append("${anime.rating}  •  ")
            if (anime.year > 0) append("${anime.year}  •  ")
            if (anime.genres.isNotEmpty()) append(anime.genres.first())
            else append(anime.provider)
        }
        cardView.contentText = details

        if (anime.posterUrl.isNotEmpty()) {
            Glide.with(cardView.context)
                .load(ua.andub.tv.ui.common.GlideUtils.buildGlideUrl(anime.posterUrl))
                .centerCrop()
                .transition(DrawableTransitionOptions.withCrossFade(200))
                .error(R.drawable.app_banner)
                .into(cardView.mainImageView)
        } else {
            cardView.mainImage = ContextCompat.getDrawable(cardView.context, R.drawable.app_banner)
        }
    }

    override fun onUnbindViewHolder(viewHolder: ViewHolder) {
        val cardView = viewHolder.view as ImageCardView
        cardView.badgeImage = null
        cardView.mainImage = null
        Glide.with(cardView.context).clear(cardView.mainImageView)
    }
}

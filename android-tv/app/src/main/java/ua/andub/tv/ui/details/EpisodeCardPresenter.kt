package ua.andub.tv.ui.details

import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.text.TextUtils
import android.view.Gravity
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.leanback.widget.Presenter
import ua.andub.tv.R
import ua.andub.tv.data.models.Episode

class EpisodeCardPresenter : Presenter() {

    override fun onCreateViewHolder(parent: ViewGroup): ViewHolder {
        val context = parent.context
        val density = context.resources.displayMetrics.density
        val padH = (14 * density).toInt()
        val padV = (10 * density).toInt()

        val root = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(padH, padV, padH, padV)
            isFocusable = true
            isFocusableInTouchMode = true
            setBackgroundResource(R.drawable.bg_card_selector)

            val width = (210 * density).toInt()
            val height = (74 * density).toInt()
            layoutParams = ViewGroup.MarginLayoutParams(width, height).apply {
                marginEnd = (14 * density).toInt()
                topMargin = (4 * density).toInt()
                bottomMargin = (4 * density).toInt()
            }

            setOnFocusChangeListener { v, hasFocus ->
                v.animate()
                    .scaleX(if (hasFocus) 1.05f else 1.0f)
                    .scaleY(if (hasFocus) 1.05f else 1.0f)
                    .translationZ(if (hasFocus) 12 * density else 0f)
                    .setDuration(160)
                    .start()
            }
        }

        // Left: Clean minimal episode number badge
        val badgeSize = (40 * density).toInt()
        val badge = TextView(context).apply {
            id = R.id.ep_badge
            textSize = 14f
            typeface = Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
            setTextColor(ContextCompat.getColor(context, R.color.text_primary))
            val bg = GradientDrawable().apply {
                shape = GradientDrawable.RECTANGLE
                cornerRadius = 8 * density
                setColor(Color.parseColor("#1f2433"))
            }
            background = bg
            layoutParams = LinearLayout.LayoutParams(badgeSize, badgeSize).apply {
                marginEnd = (12 * density).toInt()
            }
        }

        // Right: Episode label & clean subtitle
        val textContainer = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
        }

        val titleView = TextView(context).apply {
            id = R.id.ep_title
            textSize = 13f
            typeface = Typeface.DEFAULT_BOLD
            setTextColor(ContextCompat.getColor(context, R.color.text_primary))
            maxLines = 1
            ellipsize = TextUtils.TruncateAt.END
        }

        val subView = TextView(context).apply {
            id = R.id.ep_subtitle
            textSize = 11.5f
            setTextColor(ContextCompat.getColor(context, R.color.text_secondary))
            maxLines = 1
            ellipsize = TextUtils.TruncateAt.END
            setPadding(0, (2 * density).toInt(), 0, 0)
        }

        textContainer.addView(titleView)
        textContainer.addView(subView)

        root.addView(badge)
        root.addView(textContainer)

        return ViewHolder(root)
    }

    override fun onBindViewHolder(viewHolder: ViewHolder, item: Any?) {
        val episode = item as? Episode ?: return
        val root = viewHolder.view as LinearLayout
        val badge = root.findViewById<TextView>(R.id.ep_badge)
        val titleView = root.findViewById<TextView>(R.id.ep_title)
        val subView = root.findViewById<TextView>(R.id.ep_subtitle)

        badge.text = episode.number.ifEmpty { "1" }

        val rawTitle = episode.title.replace("— null", "").replace("- null", "").trim()
        val hasCustomName = rawTitle.isNotBlank() && rawTitle != "null" &&
                !rawTitle.equals("Серія ${episode.number}", ignoreCase = true)

        if (hasCustomName) {
            titleView.text = rawTitle
            subView.text = "Серія ${episode.number}"
        } else {
            titleView.text = "Серія ${episode.number}"
            subView.text = "Епізод"
        }
    }

    override fun onUnbindViewHolder(viewHolder: ViewHolder) {
        val root = viewHolder.view as LinearLayout
        root.findViewById<TextView>(R.id.ep_badge)?.text = null
        root.findViewById<TextView>(R.id.ep_title)?.text = null
        root.findViewById<TextView>(R.id.ep_subtitle)?.text = null
    }
}

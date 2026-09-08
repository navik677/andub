package ua.andub.tv.ui.browse.presenters

import android.graphics.Typeface
import android.view.Gravity
import android.view.ViewGroup
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.leanback.widget.Presenter
import ua.andub.tv.R

data class GenreItem(
    val name: String,
    val icon: String = "🏷️"
)

class GenreCardPresenter(
    private val onGenreSelected: (String) -> Unit
) : Presenter() {

    override fun onCreateViewHolder(parent: ViewGroup): ViewHolder {
        val context = parent.context
        val density = context.resources.displayMetrics.density
        val textView = TextView(context).apply {
            isFocusable = true
            isFocusableInTouchMode = true
            gravity = Gravity.CENTER
            textSize = 14f
            typeface = Typeface.DEFAULT_BOLD
            val wPx = (140 * density).toInt()
            val hPx = (52 * density).toInt()
            layoutParams = ViewGroup.MarginLayoutParams(wPx, hPx).apply {
                marginEnd = (10 * density).toInt()
            }
            setBackgroundResource(R.drawable.bg_button_pc_pill)

            setOnFocusChangeListener { v, hasFocus ->
                v.animate()
                    .scaleX(if (hasFocus) 1.08f else 1.0f)
                    .scaleY(if (hasFocus) 1.08f else 1.0f)
                    .translationZ(if (hasFocus) 10 * density else 0f)
                    .setDuration(160)
                    .start()
            }
        }
        return ViewHolder(textView)
    }

    override fun onBindViewHolder(viewHolder: ViewHolder, item: Any?) {
        val genre = when (item) {
            is GenreItem -> item.name
            is String -> item
            else -> return
        }
        val textView = viewHolder.view as TextView
        textView.text = genre
        textView.setTextColor(ContextCompat.getColor(textView.context, R.color.text_primary))

        textView.setOnClickListener {
            onGenreSelected(genre)
        }
    }

    override fun onUnbindViewHolder(viewHolder: ViewHolder) {
        val textView = viewHolder.view as TextView
        textView.text = null
    }
}

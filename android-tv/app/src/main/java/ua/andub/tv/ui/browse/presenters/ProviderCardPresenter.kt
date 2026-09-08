package ua.andub.tv.ui.browse.presenters

import android.graphics.Typeface
import android.view.Gravity
import android.view.ViewGroup
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.leanback.widget.Presenter
import ua.andub.tv.R
import ua.andub.tv.data.providers.BaseProvider
import ua.andub.tv.data.providers.ProviderRegistry

class ProviderCardPresenter(
    private val onProviderSelected: (BaseProvider) -> Unit
) : Presenter() {

    override fun onCreateViewHolder(parent: ViewGroup): ViewHolder {
        val context = parent.context
        val density = context.resources.displayMetrics.density
        val textView = TextView(context).apply {
            isFocusable = true
            isFocusableInTouchMode = true
            gravity = Gravity.CENTER
            textSize = 15f
            typeface = Typeface.DEFAULT_BOLD
            val width = context.resources.getDimensionPixelSize(R.dimen.card_width)
            val hPx = (75 * density).toInt()
            layoutParams = ViewGroup.MarginLayoutParams(width, hPx).apply {
                marginEnd = (12 * density).toInt()
            }
            setBackgroundResource(R.drawable.bg_card_selector)

            setOnFocusChangeListener { v, hasFocus ->
                v.animate()
                    .scaleX(if (hasFocus) 1.08f else 1.0f)
                    .scaleY(if (hasFocus) 1.08f else 1.0f)
                    .translationZ(if (hasFocus) 12 * density else 0f)
                    .setDuration(160)
                    .start()
            }
        }
        return ViewHolder(textView)
    }

    override fun onBindViewHolder(viewHolder: ViewHolder, item: Any?) {
        val provider = item as? BaseProvider ?: return
        val textView = viewHolder.view as TextView
        val isActive = provider.name.equals(ProviderRegistry.currentProvider.name, ignoreCase = true)

        val prefix = if (isActive) "✓  " else ""
        textView.text = "$prefix${provider.displayName}"

        if (isActive) {
            textView.setTextColor(ContextCompat.getColor(textView.context, R.color.brand_accent))
        } else {
            textView.setTextColor(ContextCompat.getColor(textView.context, R.color.text_primary))
        }

        textView.setOnClickListener {
            onProviderSelected(provider)
        }
    }

    override fun onUnbindViewHolder(viewHolder: ViewHolder) {
        val textView = viewHolder.view as TextView
        textView.text = null
    }
}

package ua.andub.tv.ui.browse

import android.content.Context
import android.graphics.drawable.Drawable
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.widget.FrameLayout
import android.widget.TextView
import androidx.leanback.widget.SearchOrbView
import androidx.leanback.widget.TitleViewAdapter
import ua.andub.tv.R

class CustomTitleView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : FrameLayout(context, attrs, defStyleAttr), TitleViewAdapter.Provider {

    private val titleView: TextView
    private val providerChip: TextView
    private val searchOrbView: SearchOrbView

    var onProviderClickListener: (() -> Unit)? = null

    init {
        LayoutInflater.from(context).inflate(R.layout.view_custom_title, this, true)
        titleView = findViewById(R.id.vTitle)
        providerChip = findViewById(R.id.vProvider)
        searchOrbView = findViewById(R.id.search_orb)

        providerChip.isFocusable = true
        providerChip.setOnClickListener {
            onProviderClickListener?.invoke()
        }
    }

    fun setProviderName(name: String) {
        providerChip.text = "$name ▾"
    }

    private val titleViewAdapter = object : TitleViewAdapter() {
        override fun getSearchAffordanceView(): View = searchOrbView

        override fun setOnSearchClickedListener(listener: OnClickListener?) {
            searchOrbView.setOnClickListener(listener)
        }

        override fun setTitle(titleText: CharSequence?) {
            titleView.text = titleText ?: "ANDUB"
        }

        override fun setBadgeDrawable(drawable: Drawable?) {
            // No-op for custom branding
        }

        override fun updateComponentsVisibility(flags: Int) {
            searchOrbView.visibility = if ((flags and SEARCH_VIEW_VISIBLE) == SEARCH_VIEW_VISIBLE) {
                View.VISIBLE
            } else {
                View.INVISIBLE
            }
        }
    }

    override fun getTitleViewAdapter(): TitleViewAdapter = titleViewAdapter
}

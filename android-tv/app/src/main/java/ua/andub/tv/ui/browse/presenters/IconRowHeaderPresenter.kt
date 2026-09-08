package ua.andub.tv.ui.browse.presenters

import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.leanback.widget.Presenter
import androidx.leanback.widget.Row
import androidx.leanback.widget.RowHeaderPresenter
import ua.andub.tv.R
import ua.andub.tv.ui.browse.rows.IconHeaderItem

class IconRowHeaderPresenter : RowHeaderPresenter() {

    override fun onCreateViewHolder(parent: ViewGroup): Presenter.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(R.layout.item_icon_header, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(viewHolder: Presenter.ViewHolder, item: Any?) {
        val headerItem = (item as? Row)?.headerItem as? IconHeaderItem
        val textView = (viewHolder as? ViewHolder)?.view?.findViewById<TextView>(R.id.header_text)
            ?: (viewHolder as? ViewHolder)?.view as? TextView

        if (headerItem != null && textView != null) {
            textView.text = headerItem.name
            if (headerItem.iconResId != 0) {
                val drawable = ContextCompat.getDrawable(textView.context, headerItem.iconResId)
                textView.setCompoundDrawablesWithIntrinsicBounds(drawable, null, null, null)
            } else {
                textView.setCompoundDrawablesWithIntrinsicBounds(null, null, null, null)
            }
        }
    }

    override fun onUnbindViewHolder(viewHolder: Presenter.ViewHolder) {
        val textView = (viewHolder as? ViewHolder)?.view?.findViewById<TextView>(R.id.header_text)
            ?: (viewHolder as? ViewHolder)?.view as? TextView
        textView?.text = null
        textView?.setCompoundDrawablesWithIntrinsicBounds(null, null, null, null)
    }
}

package ua.andub.tv.ui.search

import android.content.Context
import android.content.Intent
import android.os.Bundle
import androidx.fragment.app.FragmentActivity

class SearchTvActivity : FragmentActivity() {

    companion object {
        const val EXTRA_GENRE = "extra_genre"
        const val EXTRA_QUERY = "extra_query"

        fun start(context: Context) {
            val intent = Intent(context, SearchTvActivity::class.java)
            context.startActivity(intent)
        }

        fun startWithGenre(context: Context, genre: String) {
            val intent = Intent(context, SearchTvActivity::class.java).apply {
                putExtra(EXTRA_GENRE, genre)
            }
            context.startActivity(intent)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (savedInstanceState == null) {
            val genre = intent.getStringExtra(EXTRA_GENRE)
            val query = intent.getStringExtra(EXTRA_QUERY)
            val fragment = SearchTvFragment.newInstance(genre = genre, initialQuery = query)
            supportFragmentManager.beginTransaction()
                .replace(android.R.id.content, fragment)
                .commit()
        }
    }
}

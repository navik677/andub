package ua.andub.tv.data.providers

import okhttp3.Cookie
import okhttp3.CookieJar
import okhttp3.HttpUrl
import okhttp3.OkHttpClient
import java.util.concurrent.TimeUnit

object NetworkClient {
    private val cookieStore = HashMap<String, MutableList<Cookie>>()

    val client: OkHttpClient = OkHttpClient.Builder()
        .connectTimeout(20, TimeUnit.SECONDS)
        .readTimeout(25, TimeUnit.SECONDS)
        .followRedirects(true)
        .followSslRedirects(true)
        .cookieJar(object : CookieJar {
            override fun saveFromResponse(url: HttpUrl, cookies: List<Cookie>) {
                val host = url.host
                synchronized(cookieStore) {
                    val list = cookieStore.getOrPut(host) { mutableListOf() }
                    for (c in cookies) {
                        list.removeAll { it.name == c.name }
                        list.add(c)
                    }
                }
            }

            override fun loadForRequest(url: HttpUrl): List<Cookie> {
                synchronized(cookieStore) {
                    val host = url.host
                    val result = mutableListOf<Cookie>()
                    // Direct host cookies
                    cookieStore[host]?.let { result.addAll(it) }
                    // Domain-wide cookies (e.g. anitube.in.ua for subdomains or vice versa)
                    for ((storedHost, list) in cookieStore) {
                        if (storedHost != host && (host.endsWith(".$storedHost") || storedHost.endsWith(".$host"))) {
                            for (c in list) {
                                if (result.none { it.name == c.name }) {
                                    result.add(c)
                                }
                            }
                        }
                    }
                    return result
                }
            }
        })
        .build()

    const val USER_AGENT =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
}

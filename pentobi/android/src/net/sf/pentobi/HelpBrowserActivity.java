package net.sf.pentobi;

import net.sf.pentobi.R;
import android.app.Activity;
import android.os.Bundle;
import android.net.Uri;
import android.webkit.WebResourceRequest;
import android.webkit.WebView;
import android.webkit.WebViewClient;

public class HelpBrowserActivity
    extends Activity
{
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        try
        {
            super.onCreate(savedInstanceState);
            setContentView(R.layout.help_browser_activity);
            WebView webView = (WebView)findViewById(R.id.webview);
            webView.getSettings().setAllowFileAccess(true);
            webView.setWebViewClient(new WebViewClient() {
                @Override
                public boolean shouldOverrideUrlLoading(
                                 WebView view, WebResourceRequest request) {
                    return false;
                }
            });
            webView.loadUrl(getIntent().getData().toString());
        }
        catch (Exception e)
        {
        }
    }
}

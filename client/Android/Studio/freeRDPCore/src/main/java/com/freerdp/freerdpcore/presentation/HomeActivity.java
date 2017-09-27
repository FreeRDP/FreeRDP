/*
   Main/Home Activity

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnCreateContextMenuListener;
import android.widget.AdapterView;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ListView;

import com.freerdp.freerdpcore.R;
import com.freerdp.freerdpcore.application.GlobalApp;
import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.domain.ConnectionReference;
import com.freerdp.freerdpcore.domain.PlaceholderBookmark;
import com.freerdp.freerdpcore.domain.QuickConnectBookmark;
import com.freerdp.freerdpcore.utils.BookmarkArrayAdapter;
import com.freerdp.freerdpcore.utils.SeparatedListAdapter;

import java.util.ArrayList;

public class HomeActivity extends AppCompatActivity {
    private final static String ADD_BOOKMARK_PLACEHOLDER = "add_bookmark";
    private static final String TAG = "HomeActivity";
    private static final String PARAM_SUPERBAR_TEXT = "superbar_text";
    private ListView listViewBookmarks;
    private Button clearTextButton;
    private EditText superBarEditText;
    private BookmarkArrayAdapter manualBookmarkAdapter;
    private SeparatedListAdapter separatedListAdapter;
    private PlaceholderBookmark addBookmarkPlaceholder;
    private String sectionLabelBookmarks;

    View mDecor;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        setTitle(R.string.title_home);
        super.onCreate(savedInstanceState);
        setContentView(R.layout.home);

        mDecor = getWindow().getDecorView();
        mDecor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);

        long heapSize = Runtime.getRuntime().maxMemory();
        Log.i(TAG, "Max HeapSize: " + heapSize);
        Log.i(TAG, "App data folder: " + getFilesDir().toString());

        // load strings
        sectionLabelBookmarks = getResources().getString(R.string.section_bookmarks);

        // create add bookmark/quick connect bookmark placeholder
        addBookmarkPlaceholder = new PlaceholderBookmark();
        addBookmarkPlaceholder.setName(ADD_BOOKMARK_PLACEHOLDER);
        addBookmarkPlaceholder.setLabel(getResources().getString(R.string.list_placeholder_add_bookmark));

        // check for passed .rdp file and open it in a new bookmark
        Intent caller = getIntent();
        Uri callParameter = caller.getData();

        if (Intent.ACTION_VIEW.equals(caller.getAction()) && callParameter != null) {
            String refStr = ConnectionReference.getFileReference(callParameter.getPath());
            Bundle bundle = new Bundle();
            bundle.putString(BookmarkActivity.PARAM_CONNECTION_REFERENCE, refStr);

            Intent bookmarkIntent = new Intent(this.getApplicationContext(), BookmarkActivity.class);
            bookmarkIntent.putExtras(bundle);
            startActivity(bookmarkIntent);
        }

        // load views
        clearTextButton = (Button) findViewById(R.id.clear_search_btn);
        superBarEditText = (EditText) findViewById(R.id.superBarEditText);

        listViewBookmarks = (ListView) findViewById(R.id.listViewBookmarks);

        // set listeners for the list view
        listViewBookmarks.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                String curSection = separatedListAdapter.getSectionForPosition(position);
                Log.v(TAG, "Clicked on item id " + separatedListAdapter.getItemId(position) + " in section " + curSection);
                if (curSection == sectionLabelBookmarks) {
                    String refStr = view.getTag().toString();
                    if (ConnectionReference.isManualBookmarkReference(refStr) ||
                            ConnectionReference.isHostnameReference(refStr)) {
                        Bundle bundle = new Bundle();
                        bundle.putString(SessionActivity.PARAM_CONNECTION_REFERENCE, refStr);

                        Intent sessionIntent = new Intent(view.getContext(), SessionActivity.class);
                        sessionIntent.putExtras(bundle);
                        startActivity(sessionIntent);

                        // clear any search text
                        superBarEditText.setText("");
                        superBarEditText.clearFocus();
                    } else if (ConnectionReference.isPlaceholderReference(refStr)) {
                        // is this the add bookmark placeholder?
                        if (ConnectionReference.getPlaceholder(refStr).equals(ADD_BOOKMARK_PLACEHOLDER)) {
                            Intent bookmarkIntent = new Intent(view.getContext(), BookmarkActivity.class);
                            startActivity(bookmarkIntent);
                        }
                    }
                }
            }
        });

        listViewBookmarks.setOnCreateContextMenuListener(new OnCreateContextMenuListener() {
            @Override
            public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
                // if the selected item is not a session item (tag == null) and not a quick connect entry
                // (not a hostname connection reference) inflate the context menu
                View itemView = ((AdapterContextMenuInfo) menuInfo).targetView;
                String refStr = itemView.getTag() != null ? itemView.getTag().toString() : null;
                if (refStr != null && !ConnectionReference.isHostnameReference(refStr) && !ConnectionReference.isPlaceholderReference(refStr)) {
                    getMenuInflater().inflate(R.menu.bookmark_context_menu, menu);
                    menu.setHeaderTitle(getResources().getString(R.string.menu_title_bookmark));
                }
            }
        });

        superBarEditText.addTextChangedListener(new SuperBarTextWatcher());

        clearTextButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                superBarEditText.setText("");
            }
        });
    }


    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        // ignore orientation/keyboard change
        super.onConfigurationChanged(newConfig);
        mDecor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }

    @Override
    public boolean onSearchRequested() {
        superBarEditText.requestFocus();
        return true;
    }

    @Override
    public boolean onContextItemSelected(MenuItem aItem) {

        // get connection reference
        AdapterContextMenuInfo menuInfo = (AdapterContextMenuInfo) aItem.getMenuInfo();
        String refStr = menuInfo.targetView.getTag().toString();

        // refer to http://tools.android.com/tips/non-constant-fields why we can't use switch/case here ..
        int itemId = aItem.getItemId();
        if (itemId == R.id.bookmark_connect) {
            Bundle bundle = new Bundle();
            bundle.putString(SessionActivity.PARAM_CONNECTION_REFERENCE, refStr);
            Intent sessionIntent = new Intent(this, SessionActivity.class);
            sessionIntent.putExtras(bundle);

            startActivity(sessionIntent);
            return true;
        } else if (itemId == R.id.bookmark_edit) {
            Bundle bundle = new Bundle();
            bundle.putString(BookmarkActivity.PARAM_CONNECTION_REFERENCE, refStr);

            Intent bookmarkIntent = new Intent(this.getApplicationContext(), BookmarkActivity.class);
            bookmarkIntent.putExtras(bundle);
            startActivity(bookmarkIntent);
            return true;
        } else if (itemId == R.id.bookmark_delete) {
            if (ConnectionReference.isManualBookmarkReference(refStr)) {
                long id = ConnectionReference.getManualBookmarkId(refStr);
                GlobalApp.getManualBookmarkGateway().delete(id);
                manualBookmarkAdapter.remove(id);
                separatedListAdapter.notifyDataSetChanged();
            } else {
                assert false;
            }

            // clear super bar text
            superBarEditText.setText("");
            return true;
        }

        return false;
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.v(TAG, "HomeActivity.onResume");

        // create bookmark cursor adapter
        manualBookmarkAdapter = new BookmarkArrayAdapter(this, R.layout.bookmark_list_item, GlobalApp.getManualBookmarkGateway().findAll());

        // add add bookmark item to manual adapter
        manualBookmarkAdapter.insert(addBookmarkPlaceholder, 0);

        // attach all adapters to the separatedListView adapter and assign it to the list view
        separatedListAdapter = new SeparatedListAdapter(this);
        separatedListAdapter.addSection(sectionLabelBookmarks, manualBookmarkAdapter);
        listViewBookmarks.setAdapter(separatedListAdapter);

        // if we have a filter text entered cause an update to be caused here
        String filter = superBarEditText.getText().toString();
        if (filter.length() > 0)
            superBarEditText.setText(filter);
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.v(TAG, "HomeActivity.onPause");

        // reset adapters
        listViewBookmarks.setAdapter(null);
        separatedListAdapter = null;
        manualBookmarkAdapter = null;
    }

    @Override
    public void onBackPressed() {
        // if back was pressed - ask the user if he really wants to exit
        if (ApplicationSettingsActivity.getAskOnExit(this)) {
            final CheckBox cb = new CheckBox(this);
            cb.setChecked(!ApplicationSettingsActivity.getAskOnExit(this));
            cb.setText(R.string.dlg_dont_show_again);

            AlertDialog.Builder builder = new AlertDialog.Builder(this);
            builder.setTitle(R.string.dlg_title_exit)
                    .setMessage(R.string.dlg_msg_exit)
                    .setView(cb)
                    .setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface dialog, int which) {
                            finish();
                        }
                    })
                    .setNegativeButton(R.string.no, new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface dialog, int which) {
                            dialog.dismiss();
                        }
                    })
                    .create()
                    .show();
        } else {
            super.onBackPressed();
        }
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putString(PARAM_SUPERBAR_TEXT, superBarEditText.getText().toString());
    }

    @Override
    protected void onRestoreInstanceState(Bundle inState) {
        super.onRestoreInstanceState(inState);
        superBarEditText.setText(inState.getString(PARAM_SUPERBAR_TEXT));
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.home_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {

        // refer to http://tools.android.com/tips/non-constant-fields why we can't use switch/case here ..
        int itemId = item.getItemId();
        if (itemId == R.id.newBookmark) {
            Intent bookmarkIntent = new Intent(this, BookmarkActivity.class);
            startActivity(bookmarkIntent);
        } else if (itemId == R.id.appSettings) {
            Intent settingsIntent = new Intent(this, ApplicationSettingsActivity.class);
            startActivity(settingsIntent);
        } else if (itemId == R.id.help) {
            Intent helpIntent = new Intent(this, HelpActivity.class);
            startActivity(helpIntent);
        } else if (itemId == R.id.about) {
            Intent aboutIntent = new Intent(this, AboutActivity.class);
            startActivity(aboutIntent);
        }

        return true;
    }

    private class SuperBarTextWatcher implements TextWatcher {
        @Override
        public void afterTextChanged(Editable s) {
            if (separatedListAdapter != null) {
                String text = s.toString();
                if (text.length() > 0) {
                    ArrayList<BookmarkBase> computers_list = GlobalApp.getQuickConnectHistoryGateway().findHistory(text);
                    computers_list.addAll(GlobalApp.getManualBookmarkGateway().findByLabelOrHostnameLike(text));
                    manualBookmarkAdapter.replaceItems(computers_list);
                    QuickConnectBookmark qcBm = new QuickConnectBookmark();
                    qcBm.setLabel(text);
                    qcBm.setHostname(text);
                    manualBookmarkAdapter.insert(qcBm, 0);
                } else {
                    manualBookmarkAdapter.replaceItems(GlobalApp.getManualBookmarkGateway().findAll());
                    manualBookmarkAdapter.insert(addBookmarkPlaceholder, 0);
                }

                separatedListAdapter.notifyDataSetChanged();
            }
        }

        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) {
        }

        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
        }
    }
}

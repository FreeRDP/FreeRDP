package com.freerdp.freerdpcore.utils;

import android.content.res.Configuration;
import android.os.Bundle;
import android.preference.PreferenceActivity;
import androidx.annotation.Nullable;
import androidx.annotation.NonNull;
import androidx.annotation.LayoutRes;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.appcompat.widget.Toolbar;
import android.view.MenuInflater;
import android.view.View;
import android.view.ViewGroup;

public abstract class AppCompatPreferenceActivity extends PreferenceActivity
{

	private AppCompatDelegate mDelegate;

	@Override protected void onCreate(Bundle savedInstanceState)
	{
		getDelegate().installViewFactory();
		getDelegate().onCreate(savedInstanceState);
		super.onCreate(savedInstanceState);
	}

	@Override protected void onPostCreate(Bundle savedInstanceState)
	{
		super.onPostCreate(savedInstanceState);
		getDelegate().onPostCreate(savedInstanceState);
	}

	public ActionBar getSupportActionBar()
	{
		return getDelegate().getSupportActionBar();
	}

	public void setSupportActionBar(@Nullable Toolbar toolbar)
	{
		getDelegate().setSupportActionBar(toolbar);
	}

	@Override @NonNull public MenuInflater getMenuInflater()
	{
		return getDelegate().getMenuInflater();
	}

	@Override public void setContentView(@LayoutRes int layoutResID)
	{
		getDelegate().setContentView(layoutResID);
	}

	@Override public void setContentView(View view)
	{
		getDelegate().setContentView(view);
	}

	@Override public void setContentView(View view, ViewGroup.LayoutParams params)
	{
		getDelegate().setContentView(view, params);
	}

	@Override public void addContentView(View view, ViewGroup.LayoutParams params)
	{
		getDelegate().addContentView(view, params);
	}

	@Override protected void onPostResume()
	{
		super.onPostResume();
		getDelegate().onPostResume();
	}

	@Override protected void onTitleChanged(CharSequence title, int color)
	{
		super.onTitleChanged(title, color);
		getDelegate().setTitle(title);
	}

	@Override public void onConfigurationChanged(Configuration newConfig)
	{
		super.onConfigurationChanged(newConfig);
		getDelegate().onConfigurationChanged(newConfig);
	}

	@Override protected void onStop()
	{
		super.onStop();
		getDelegate().onStop();
	}

	@Override protected void onDestroy()
	{
		super.onDestroy();
		getDelegate().onDestroy();
	}

	public void invalidateOptionsMenu()
	{
		getDelegate().invalidateOptionsMenu();
	}

	private AppCompatDelegate getDelegate()
	{
		if (mDelegate == null)
		{
			mDelegate = AppCompatDelegate.create(this, null);
		}
		return mDelegate;
	}
}
/*
   RecyclerView adapter for bookmark list items

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.utils;

import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.appcompat.widget.PopupMenu;
import androidx.recyclerview.widget.RecyclerView;

import com.freerdp.freerdpcore.R;
import com.freerdp.freerdpcore.databinding.BookmarkListItemBinding;
import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.domain.ConnectionReference;
import com.freerdp.freerdpcore.presentation.BookmarkActivity;

import java.util.ArrayList;
import java.util.List;

public class BookmarkListAdapter extends RecyclerView.Adapter<BookmarkListAdapter.ViewHolder>
{
	public interface Callbacks
	{
		void onItemClick(String refStr);
		void onDelete(long id);
		void onExport(BookmarkBase bookmark);
	}

	private List<BookmarkBase> items = new ArrayList<>();
	private Callbacks callbacks;
	private boolean actionsEnabled = true;

	public void setCallbacks(Callbacks callbacks)
	{
		this.callbacks = callbacks;
	}

	public void setActionsEnabled(boolean enabled)
	{
		actionsEnabled = enabled;
	}

	public List<BookmarkBase> getItems()
	{
		return items;
	}

	public void setItems(List<BookmarkBase> newItems)
	{
		items = newItems != null ? newItems : new ArrayList<>();
		notifyDataSetChanged();
	}

	@NonNull @Override public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType)
	{
		BookmarkListItemBinding binding = BookmarkListItemBinding.inflate(
		    LayoutInflater.from(parent.getContext()), parent, false);
		return new ViewHolder(binding);
	}

	@Override public void onBindViewHolder(@NonNull ViewHolder holder, int position)
	{
		BookmarkBase bookmark = items.get(position);
		String refStr;

		holder.binding.bookmarkText1.setText(bookmark.getLabel());

		if (bookmark.getType() == BookmarkBase.TYPE_MANUAL)
		{
			holder.binding.bookmarkIcon1.setImageResource(R.drawable.ic_computer);
			holder.binding.bookmarkIcon1.setVisibility(View.VISIBLE);
			holder.binding.bookmarkText2.setVisibility(View.VISIBLE);
			holder.binding.bookmarkText2.setText(bookmark.getHostname());
			refStr = ConnectionReference.getBookmarkReference(bookmark.getId());

			if (actionsEnabled)
			{
				holder.binding.bookmarkIcon2.setVisibility(View.VISIBLE);
				holder.binding.bookmarkIcon2.setImageResource(R.drawable.ic_more_vert);
				final String finalRefStr = refStr;
				final long bookmarkId = bookmark.getId();
				holder.binding.bookmarkIcon2.setOnClickListener(
				    v -> showBookmarkMenu(v, finalRefStr, bookmarkId, bookmark));
				holder.itemView.setOnLongClickListener(v -> {
					showBookmarkMenu(holder.binding.bookmarkIcon2, finalRefStr, bookmarkId,
					                 bookmark);
					return true;
				});
			}
			else
			{
				holder.binding.bookmarkIcon2.setVisibility(View.GONE);
				holder.binding.bookmarkIcon2.setOnClickListener(null);
				holder.itemView.setOnLongClickListener(null);
			}
		}
		else if (bookmark.getType() == BookmarkBase.TYPE_QUICKCONNECT)
		{
			holder.binding.bookmarkText2.setVisibility(View.GONE);
			refStr = ConnectionReference.getHostnameReference(bookmark.getHostname());
			holder.itemView.setOnLongClickListener(null);

			if (bookmark.isDirectConnect())
			{
				holder.binding.bookmarkText1.setText(holder.itemView.getContext().getString(
				    R.string.quick_connect_to, bookmark.getHostname()));
				holder.binding.bookmarkIcon1.setImageResource(R.drawable.ic_login);
				holder.binding.bookmarkIcon1.setVisibility(View.VISIBLE);
				holder.binding.bookmarkIcon2.setVisibility(View.GONE);
				holder.binding.bookmarkIcon2.setOnClickListener(null);
			}
			else
			{
				holder.binding.bookmarkText1.setText(bookmark.getLabel());
				holder.binding.bookmarkIcon1.setImageResource(R.drawable.ic_history);
				holder.binding.bookmarkIcon1.setVisibility(View.VISIBLE);
				holder.binding.bookmarkIcon2.setVisibility(View.VISIBLE);
				holder.binding.bookmarkIcon2.setImageResource(R.drawable.ic_bookmark_add);
				final String hostname = bookmark.getHostname();
				holder.binding.bookmarkIcon2.setOnClickListener(v -> {
					Bundle bundle = new Bundle();
					bundle.putString(BookmarkActivity.PARAM_CONNECTION_REFERENCE,
					                 ConnectionReference.getHostnameReference(hostname));
					Intent intent = new Intent(v.getContext(), BookmarkActivity.class);
					intent.putExtras(bundle);
					v.getContext().startActivity(intent);
				});
			}
		}
		else
		{
			holder.binding.bookmarkIcon1.setVisibility(View.GONE);
			holder.binding.bookmarkText2.setVisibility(View.GONE);
			refStr = "";
			holder.binding.bookmarkIcon2.setVisibility(View.GONE);
			holder.binding.bookmarkIcon2.setOnClickListener(null);
		}

		holder.itemView.setTag(refStr);

		holder.itemView.setOnClickListener(v -> {
			if (callbacks != null)
				callbacks.onItemClick(v.getTag().toString());
		});
	}

	@Override public int getItemCount()
	{
		return items.size();
	}

	private void showBookmarkMenu(View anchor, String refStr, long bookmarkId,
	                              BookmarkBase bookmark)
	{
		PopupMenu popup = new PopupMenu(anchor.getContext(), anchor);
		popup.inflate(R.menu.bookmark_context_menu);
		popup.setOnMenuItemClickListener(item -> {
			int itemId = item.getItemId();
			if (itemId == R.id.bookmark_connect)
			{
				if (callbacks != null)
					callbacks.onItemClick(refStr);
				return true;
			}
			else if (itemId == R.id.bookmark_edit)
			{
				Bundle bundle = new Bundle();
				bundle.putString(BookmarkActivity.PARAM_CONNECTION_REFERENCE, refStr);
				Intent intent = new Intent(anchor.getContext(), BookmarkActivity.class);
				intent.putExtras(bundle);
				anchor.getContext().startActivity(intent);
				return true;
			}
			else if (itemId == R.id.bookmark_delete)
			{
				if (callbacks != null)
					callbacks.onDelete(bookmarkId);
				return true;
			}
			else if (itemId == R.id.bookmark_export)
			{
				if (callbacks != null)
					callbacks.onExport(bookmark);
				return true;
			}
			return false;
		});
		popup.show();
	}

	static class ViewHolder extends RecyclerView.ViewHolder
	{
		final BookmarkListItemBinding binding;

		ViewHolder(BookmarkListItemBinding binding)
		{
			super(binding.getRoot());
			this.binding = binding;
		}
	}
}

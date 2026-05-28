/*
   AES-256-GCM key-wrapping helper backed by Android Keystore.

   A random 256-bit database key is generated once, wrapped with the
   Keystore-resident master key, and stored in SharedPreferences as
   Base64( IV[12] || ciphertext ).

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.security;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;

import java.security.KeyStore;
import java.security.SecureRandom;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

public final class KeystoreHelper
{
	private static final String KEYSTORE_PROVIDER = "AndroidKeyStore";
	private static final String KEY_ALIAS = "freerdp_db_master_key";
	private static final String PREFS_NAME = "freerdp_security_prefs";
	private static final String PREF_ENCRYPTED_DB_KEY = "encrypted_db_key";
	private static final String CIPHER_TRANSFORMATION = "AES/GCM/NoPadding";
	private static final int GCM_IV_LENGTH = 12;
	private static final int GCM_TAG_LENGTH = 128; // bits
	private static final int DB_KEY_LENGTH = 32;   // bytes (256-bit)

	private static volatile KeystoreHelper instance;

	private final Context applicationContext;

	private KeystoreHelper(Context context)
	{
		this.applicationContext = context.getApplicationContext();
	}

	public static KeystoreHelper getInstance(Context context)
	{
		if (instance == null)
		{
			synchronized (KeystoreHelper.class)
			{
				if (instance == null)
				{
					instance = new KeystoreHelper(context);
				}
			}
		}
		return instance;
	}

	public byte[] getOrCreateDbKey() throws KeystoreException
	{
		SharedPreferences prefs =
		    applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
		String encoded = prefs.getString(PREF_ENCRYPTED_DB_KEY, null);

		if (encoded != null)
		{
			return decryptDbKey(encoded);
		}

		byte[] rawKey = new byte[DB_KEY_LENGTH];
		new SecureRandom().nextBytes(rawKey);
		String encrypted = encryptDbKey(rawKey);
		prefs.edit().putString(PREF_ENCRYPTED_DB_KEY, encrypted).apply();
		return rawKey;
	}

	private String encryptDbKey(byte[] rawKey) throws KeystoreException
	{
		try
		{
			SecretKey masterKey = getOrCreateMasterKey();
			Cipher cipher = Cipher.getInstance(CIPHER_TRANSFORMATION);
			cipher.init(Cipher.ENCRYPT_MODE, masterKey);
			byte[] iv = cipher.getIV();
			byte[] ciphertext = cipher.doFinal(rawKey);

			// Serialise as Base64( IV[12] || ciphertext )
			byte[] packed = new byte[GCM_IV_LENGTH + ciphertext.length];
			System.arraycopy(iv, 0, packed, 0, GCM_IV_LENGTH);
			System.arraycopy(ciphertext, 0, packed, GCM_IV_LENGTH, ciphertext.length);
			return Base64.encodeToString(packed, Base64.NO_WRAP);
		}
		catch (Exception e)
		{
			throw new KeystoreException("Failed to encrypt database key", e);
		}
	}

	private byte[] decryptDbKey(String encoded) throws KeystoreException
	{
		try
		{
			byte[] packed = Base64.decode(encoded, Base64.NO_WRAP);
			byte[] iv = new byte[GCM_IV_LENGTH];
			byte[] ciphertext = new byte[packed.length - GCM_IV_LENGTH];
			System.arraycopy(packed, 0, iv, 0, GCM_IV_LENGTH);
			System.arraycopy(packed, GCM_IV_LENGTH, ciphertext, 0, ciphertext.length);

			SecretKey masterKey = getOrCreateMasterKey();
			Cipher cipher = Cipher.getInstance(CIPHER_TRANSFORMATION);
			cipher.init(Cipher.DECRYPT_MODE, masterKey, new GCMParameterSpec(GCM_TAG_LENGTH, iv));
			return cipher.doFinal(ciphertext);
		}
		catch (Exception e)
		{
			throw new KeystoreException("Failed to decrypt database key", e);
		}
	}

	private SecretKey getOrCreateMasterKey() throws Exception
	{
		KeyStore keyStore = KeyStore.getInstance(KEYSTORE_PROVIDER);
		keyStore.load(null);

		if (keyStore.containsAlias(KEY_ALIAS))
		{
			return ((KeyStore.SecretKeyEntry)keyStore.getEntry(KEY_ALIAS, null)).getSecretKey();
		}

		KeyGenerator keyGen =
		    KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, KEYSTORE_PROVIDER);
		keyGen.init(
		    new KeyGenParameterSpec
		        .Builder(KEY_ALIAS, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
		        .setKeySize(256)
		        .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
		        .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
		        .build());
		return keyGen.generateKey();
	}

	public static final class KeystoreException extends Exception
	{
		public KeystoreException(String message, Throwable cause)
		{
			super(message, cause);
		}
	}
}

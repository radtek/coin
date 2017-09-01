﻿/*######   Copyright (c) 2011-2015 Ufasoft  http://yupitecoin.com  mailto:support@yupitecoin.com,  Sergey Pavlov  mailto:dev@yupitecoin.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Globalization;

using Utils;
using GuiComp;

using Interop.coineng;

namespace Coin {
	public partial class FormTransactions : Window {
		ListViewSortHelper ListViewSortHelper = new ListViewSortHelper();

		public FormTransactions() {
			InitializeComponent();

			ListViewSortHelper.ListView = lv;
		}

		WalletForms m_walletForms;

		public WalletForms WalletForms {
			get { return m_walletForms; }
			set {
				m_walletForms = value;
				Title = WalletForms.Wallet.CurrencyName + " Transactions";
			}
		}

		IWallet Wallet {
			get { return WalletForms.Wallet; }
		}

		public ObservableCollection<Tx> Transactions = new ObservableCollection<Tx>();
		
		Tx GetSelectedTransaction() {
			return (Tx)lv.SelectedItem;
		}

		public void UpdateTransactions() {
			ITransaction iSelectedTx = null;
			if (lv.SelectedItem != null)
				iSelectedTx = (lv.SelectedItem as Tx).m_iTx;
			Transactions.Clear();
			foreach (var tx in Wallet.Transactions) {
				var x = new Tx() { m_iTx = tx };
				Transactions.Add(x);
				if (iSelectedTx == tx)
					lv.SelectedItem = x;
			}
		}

		private void Window_Loaded(object sender, RoutedEventArgs e) {
			UpdateTransactions();
			lv.ItemsSource = Transactions;
		}

		private void FormTransactions_Closed(object sender, EventArgs e) {
			WalletForms.FormTransactions = null;
		}

		void ShowTxInfo(Tx tx) {
			var dlg = new DialogTextInfo();
			dlg.Title = "Transaction Info";
			dlg.textInfo.Text = string.Format("DateTime:  {0}\nValue:   {1}\nFee:  {6}\nConfirmations: {2}\nTo:   {3}\nComment:  {4}\nHash:   {5}", tx.Timestamp, tx.m_iTx.Amount, tx.Confirmations.ToString(), tx.Address + " " + tx.m_iTx.Address.Comment, tx.m_iTx.Comment, tx.Hash, tx.Fee);
            dlg.Width = 600;
            dlg.Height = 200;
            Dialog.ShowDialog(dlg, this);
		}

		private void lv_MouseDoubleClick(object sender, MouseButtonEventArgs e) {
			ShowTxInfo(GetSelectedTransaction());
		}

		private void OnTxInfo(object sender, RoutedEventArgs e) {
			ShowTxInfo(GetSelectedTransaction());
		}

		private void OnEditComment(object sender, RoutedEventArgs e) {
			var tx = GetSelectedTransaction();
			string r = FormQueryString.QueryString("Edit Comment", tx.Comment);
			if (r != null) {
				tx.m_iTx.Comment = r;
				UpdateTransactions();
			}
		}

		
	}

	public class Tx {
		internal ITransaction m_iTx;

		public DateTime Timestamp { get { return m_iTx.Timestamp.ToLocalTime(); } }

		public string Debit {
			get {
				var v = m_iTx.Amount;
				return v < 0 ? v.ToString() : "";
			}
		}

		public string Credit {
			get {
				var v = m_iTx.Amount;
				return v > 0 ? v.ToString() : "";
			}
		}

		public string Fee {
			get {
				try {
					var v = m_iTx.Fee;
					return v != 0 ? v.ToString() : "";
				} catch (Exception) {
					return "Unknown";
				}
			}
		}

		public string Address { get { return m_iTx.Address.Value; } }
		public int Confirmations { get { return m_iTx.Confirmations; } }
		public string Hash { get { return m_iTx.Hash; } }
		
		public string Comment { get {
			string addrComment = m_iTx.Address.Comment;
			return (addrComment == "" ? "" : "[" + addrComment + "] ") + m_iTx.Comment;
		} }

	};

	public class DateTimeConverter : IValueConverter {
		public object Convert(object value, Type targetType, object parameter, CultureInfo culture) {
			if (value == null)
				return null;
			DateTime dateTime = (DateTime)value;
			return dateTime.ToString(CultureInfo.CurrentCulture);
		}
		public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) {
			throw new NotImplementedException();
		}
	}
}

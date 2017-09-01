﻿/*######   Copyright (c) 2011-2015 Ufasoft  http://yupitecoin.com  mailto:support@yupitecoin.com,  Sergey Pavlov  mailto:dev@yupitecoin.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;

namespace Coin {
	public partial class FormOptions : Window {
		public FormOptions(FormMain fmain) {
			InitializeComponent();
			cbProxy.SelectedIndex = 0;
			if (fmain.Eng.SupportsTOR)
				cbProxy.Items.Add("TOR");			
        }

		private void OnOk(object sender, RoutedEventArgs e) {
			DialogResult = true;
		}

		void OnProxyTypeChanged(object sender, RoutedEventArgs e) {
			editProxy.IsEnabled = cbProxy.SelectedIndex == 1 || cbProxy.SelectedIndex == 2;
		}
	}
}

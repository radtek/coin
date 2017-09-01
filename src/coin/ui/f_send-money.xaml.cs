/*######   Copyright (c) 2011-2015 Ufasoft  http://yupitecoin.com  mailto:support@yupitecoin.com,  Sergey Pavlov  mailto:dev@yupitecoin.com ####
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


namespace Coin {
	public partial class FormSendMoney : Window {
		public FormSendMoney() {
			InitializeComponent();
		}

		

        private void Window_Closed(object sender, EventArgs e) {
			if (DialogResult == true) {
				
			}
		}

		
	}
}

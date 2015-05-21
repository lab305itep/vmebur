void doplots(char *name, int xil)
{
	TFile *f;
	TCanvas *cv;
	TH1D *h;
	char str[1024];
	int i;
	
	sprintf(str, "%s.root", name);
	f = new TFile(str);
	if (!f->IsOpen()) return;
	
	cv = new TCanvas("CV", "Plots", 2000, 3000);
	
	for (i=0; i<15; i++) {
		if ((i%6) == 0) {
			cv->Clear();
			cv->Divide(2, 3);
		}
		cv->cd((i%6)+1);
		sprintf(str, "HA%2.2d", i + 16*xil);
		h = (TH1D *)f->Get(str);
		if (h) {
			h->GetXaxis()->SetRange(80, 200);
			h->Draw();
		}
		switch(i) {
		case 5:
			sprintf(str, "%s.pdf(", name);
			break;
		case 11:
			sprintf(str, "%s.pdf(", name);
			break;
		case 14:
			sprintf(str, "%s.pdf)", name);
			break;
		default:
			str[0] = '\0';
		}
		if (str[0]) cv->SaveAs(str);
	}
	f->Close();
}

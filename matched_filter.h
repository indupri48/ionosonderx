void matched_filter_create(float f0, float f1, float rate, float sample_rate);

void matched_filter_execute(float *samples_in, float *samples_out);

void matched_filter_destroy();

int matched_filter_length();

int get_n_input_samples();